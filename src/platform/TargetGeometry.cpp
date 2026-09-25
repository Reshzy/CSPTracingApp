#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "platform/TargetGeometry.h"

#include <dwmapi.h>

#include <atomic>

#ifndef EVENT_OBJECT_CLOAKED
#define EVENT_OBJECT_CLOAKED 0x8017
#endif
#ifndef EVENT_OBJECT_UNCLOAKED
#define EVENT_OBJECT_UNCLOAKED 0x8018
#endif

namespace tracing::platform {
namespace {

class UniqueHandle
{
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle)
    {
    }

    ~UniqueHandle()
    {
        reset();
    }

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    UniqueHandle& operator=(UniqueHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    UniqueHandle(UniqueHandle const&) = delete;
    UniqueHandle& operator=(UniqueHandle const&) = delete;

    HANDLE get() const noexcept
    {
        return handle_;
    }

    explicit operator bool() const noexcept
    {
        return handle_ != nullptr;
    }

    void reset() noexcept
    {
        if (handle_ != nullptr)
        {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

private:
    HANDLE handle_ = nullptr;
};

PhysicalRect FromPoints(POINT const& topLeft, POINT const& bottomRight) noexcept
{
    PhysicalRect rect{};
    rect.x = topLeft.x;
    rect.y = topLeft.y;
    rect.width = bottomRight.x - topLeft.x;
    rect.height = bottomRight.y - topLeft.y;
    return rect;
}

PhysicalRect FromWinRect(RECT const& value) noexcept
{
    PhysicalRect rect{};
    rect.x = value.left;
    rect.y = value.top;
    rect.width = value.right - value.left;
    rect.height = value.bottom - value.top;
    return rect;
}

bool QueryCreationTime(std::uint32_t pid, std::uint64_t& creationTime, std::wstring& error)
{
    UniqueHandle const process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process)
    {
        unsigned long const code = GetLastError();
        error = FormatAccessFailure(pid, code);
        return false;
    }

    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(process.get(), &created, &exited, &kernel, &user) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = FormatAccessFailure(pid, code);
        return false;
    }

    ULARGE_INTEGER value{};
    value.LowPart = created.dwLowDateTime;
    value.HighPart = created.dwHighDateTime;
    creationTime = value.QuadPart;
    return true;
}

bool IsWatchedEvent(DWORD event) noexcept
{
    switch (event)
    {
    case EVENT_OBJECT_LOCATIONCHANGE:
    case EVENT_SYSTEM_MINIMIZESTART:
    case EVENT_SYSTEM_MINIMIZEEND:
    case EVENT_OBJECT_DESTROY:
    case EVENT_OBJECT_HIDE:
    case EVENT_OBJECT_SHOW:
    case EVENT_SYSTEM_FOREGROUND:
    case EVENT_OBJECT_CLOAKED:
    case EVENT_OBJECT_UNCLOAKED:
        return true;
    default:
        return false;
    }
}

std::atomic<HWND> g_notifyWindow{nullptr};
std::atomic<HWND> g_targetWindow{nullptr};
std::atomic<UINT> g_notifyMessage{0};

void CALLBACK GeometryWinEventProc(
    HWINEVENTHOOK,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD,
    DWORD)
{
    if (!IsWatchedEvent(event))
    {
        return;
    }
    if (idChild != CHILDID_SELF)
    {
        return;
    }
    if (event != EVENT_SYSTEM_FOREGROUND && idObject != OBJID_WINDOW && idObject != OBJID_CLIENT)
    {
        return;
    }

    HWND const notify = g_notifyWindow.load(std::memory_order_acquire);
    if (notify == nullptr)
    {
        return;
    }

    HWND const target = g_targetWindow.load(std::memory_order_acquire);
    if (event != EVENT_SYSTEM_FOREGROUND && target != nullptr && hwnd != target)
    {
        return;
    }

    UINT const message = g_notifyMessage.load(std::memory_order_acquire);
    if (message == 0)
    {
        return;
    }

    PostMessageW(notify, message, static_cast<WPARAM>(event), reinterpret_cast<LPARAM>(hwnd));
}

} // namespace

bool PhysicalGeometryChanged(GeometrySnapshot const& previous, GeometrySnapshot const& next) noexcept
{
    return previous.clientPhysical.x != next.clientPhysical.x ||
           previous.clientPhysical.y != next.clientPhysical.y ||
           previous.clientPhysical.width != next.clientPhysical.width ||
           previous.clientPhysical.height != next.clientPhysical.height ||
           previous.dpi != next.dpi;
}

std::uint64_t NextGeometryGeneration(
    GeometrySnapshot const& previous,
    GeometrySnapshot const& sampled) noexcept
{
    if (sampled.targetGeneration == 0 || !sampled.hwndValid || !sampled.identityValid)
    {
        return 0;
    }
    if (previous.targetGeneration != sampled.targetGeneration || previous.geometryGeneration == 0)
    {
        return 1;
    }
    if (PhysicalGeometryChanged(previous, sampled))
    {
        return previous.geometryGeneration + 1;
    }
    return previous.geometryGeneration;
}

OverlayEligibility ClassifyOverlayEligibility(GeometrySnapshot const& snapshot) noexcept
{
    if (!snapshot.hwndValid || snapshot.identity == IdentityCheck::WindowDead)
    {
        return OverlayEligibility::TargetDead;
    }
    if (!snapshot.identityValid || snapshot.identity != IdentityCheck::Match)
    {
        return OverlayEligibility::IdentityMismatch;
    }
    if (snapshot.minimized)
    {
        return OverlayEligibility::Minimized;
    }
    if (snapshot.cloaked)
    {
        return OverlayEligibility::Cloaked;
    }
    if (snapshot.clientPhysical.width <= 0 || snapshot.clientPhysical.height <= 0)
    {
        return OverlayEligibility::InvalidGeometry;
    }
    if (!snapshot.targetForeground)
    {
        return OverlayEligibility::NotForeground;
    }
    return OverlayEligibility::Eligible;
}

std::wstring FormatPhysicalRect(PhysicalRect const& rect)
{
    return L"origin=(" + std::to_wstring(rect.x) + L"," + std::to_wstring(rect.y) + L") size=" +
           std::to_wstring(rect.width) + L"x" + std::to_wstring(rect.height);
}

std::wstring FormatOverlayEligibility(OverlayEligibility eligibility)
{
    switch (eligibility)
    {
    case OverlayEligibility::Eligible:
        return L"eligible";
    case OverlayEligibility::Minimized:
        return L"minimized";
    case OverlayEligibility::Cloaked:
        return L"cloaked";
    case OverlayEligibility::NotForeground:
        return L"not-foreground";
    case OverlayEligibility::InvalidGeometry:
        return L"invalid-geometry";
    case OverlayEligibility::TargetDead:
        return L"target-dead";
    case OverlayEligibility::IdentityMismatch:
        return L"identity-mismatch";
    }
    return L"unknown";
}

std::wstring FormatGeometryReport(GeometrySnapshot const& snapshot)
{
    OverlayEligibility const eligibility = ClassifyOverlayEligibility(snapshot);
    std::wstring text;
    text += L"target gen ";
    text += std::to_wstring(snapshot.targetGeneration);
    text += L" geom ";
    text += std::to_wstring(snapshot.geometryGeneration);
    text += L"\r\nclient physical: ";
    text += FormatPhysicalRect(snapshot.clientPhysical);
    text += L" dpi=";
    text += std::to_wstring(snapshot.dpi);
    text += L"\r\nouter window: ";
    text += FormatPhysicalRect(snapshot.outerWindow);
    text += L"\r\ndwm frame: ";
    text += FormatPhysicalRect(snapshot.dwmFrame);
    text += L"\r\nFG=";
    text += snapshot.targetForeground ? L"yes" : L"no";
    text += L" minimized=";
    text += snapshot.minimized ? L"yes" : L"no";
    text += L" cloaked=";
    text += snapshot.cloaked ? L"yes" : L"no";
    text += L" visible=";
    text += snapshot.visible ? L"yes" : L"no";
    text += L"\r\neligibility=";
    text += FormatOverlayEligibility(eligibility);
    text += L"\r\nnot canvas bounds";
    return text;
}

bool QueryPhysicalGeometry(
    TargetIdentity const& identity,
    GeometrySnapshot& snapshot,
    std::wstring& error)
{
    snapshot = GeometrySnapshot{};
    snapshot.targetGeneration = identity.sessionGeneration;
    error.clear();

    if (identity.hwnd == nullptr || IsWindow(identity.hwnd) == FALSE)
    {
        snapshot.hwndValid = false;
        snapshot.identity = IdentityCheck::WindowDead;
        error = L"Target HWND is no longer valid.";
        return false;
    }
    snapshot.hwndValid = true;

    DWORD pid = 0;
    GetWindowThreadProcessId(identity.hwnd, &pid);
    std::uint64_t creationTime = 0;
    bool const processKnown = QueryCreationTime(static_cast<std::uint32_t>(pid), creationTime, error);
    snapshot.identity = CheckIdentity(
        identity.process.pid,
        identity.process.creationTime,
        true,
        processKnown,
        static_cast<std::uint32_t>(pid),
        creationTime);
    snapshot.identityValid = snapshot.identity == IdentityCheck::Match;
    if (!snapshot.identityValid)
    {
        if (snapshot.identity == IdentityCheck::HandleReused)
        {
            error =
                L"HWND was reused by another process. Geometry discarded so a launcher or second "
                L"instance cannot keep the previous origin or DPI.";
        }
        else if (error.empty())
        {
            error = FormatAccessFailure(identity.process.pid, ERROR_ACCESS_DENIED);
        }
        return false;
    }

    RECT client{};
    if (GetClientRect(identity.hwnd, &client) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = L"GetClientRect failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    POINT topLeft{client.left, client.top};
    POINT bottomRight{client.right, client.bottom};
    if (ClientToScreen(identity.hwnd, &topLeft) == FALSE ||
        ClientToScreen(identity.hwnd, &bottomRight) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = L"ClientToScreen failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    snapshot.clientPhysical = FromPoints(topLeft, bottomRight);

    RECT outer{};
    if (GetWindowRect(identity.hwnd, &outer) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = L"GetWindowRect failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    snapshot.outerWindow = FromWinRect(outer);

    RECT dwm{};
    if (SUCCEEDED(DwmGetWindowAttribute(identity.hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm, sizeof(dwm))))
    {
        snapshot.dwmFrame = FromWinRect(dwm);
    }

    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(identity.hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))))
    {
        snapshot.cloaked = cloaked != FALSE;
    }

    snapshot.dpi = GetDpiForWindow(identity.hwnd);
    snapshot.visible = IsWindowVisible(identity.hwnd) != FALSE;
    snapshot.minimized = IsIconic(identity.hwnd) != FALSE;
    snapshot.targetForeground = GetForegroundWindow() == identity.hwnd;
    return true;
}

TargetLifecycleWatcher::~TargetLifecycleWatcher()
{
    Detach();
}

bool TargetLifecycleWatcher::Attach(
    HWND controlWindow,
    HWND targetWindow,
    unsigned notifyMessage,
    std::wstring& error)
{
    Detach();
    error.clear();

    if (controlWindow == nullptr || targetWindow == nullptr || notifyMessage == 0)
    {
        error = L"Cannot watch geometry: control window, target HWND, or notify message is missing.";
        return false;
    }

    g_notifyWindow.store(controlWindow, std::memory_order_release);
    g_targetWindow.store(targetWindow, std::memory_order_release);
    g_notifyMessage.store(notifyMessage, std::memory_order_release);

    DWORD const flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    systemHook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_MINIMIZEEND,
        nullptr,
        GeometryWinEventProc,
        0,
        0,
        flags);
    objectHook_ = SetWinEventHook(
        EVENT_OBJECT_DESTROY,
        EVENT_OBJECT_UNCLOAKED,
        nullptr,
        GeometryWinEventProc,
        0,
        0,
        flags);

    if (systemHook_ == nullptr || objectHook_ == nullptr)
    {
        unsigned long const code = GetLastError();
        ResetHooks();
        g_notifyWindow.store(nullptr, std::memory_order_release);
        g_targetWindow.store(nullptr, std::memory_order_release);
        g_notifyMessage.store(0, std::memory_order_release);
        error = L"SetWinEventHook failed (Win32 " + std::to_wstring(code) +
                L"). Bounded polling can still refresh geometry.";
        return false;
    }
    return true;
}

void TargetLifecycleWatcher::Detach() noexcept
{
    ResetHooks();
    g_notifyWindow.store(nullptr, std::memory_order_release);
    g_targetWindow.store(nullptr, std::memory_order_release);
    g_notifyMessage.store(0, std::memory_order_release);
}

bool TargetLifecycleWatcher::IsAttached() const noexcept
{
    return systemHook_ != nullptr && objectHook_ != nullptr;
}

void TargetLifecycleWatcher::ResetHooks() noexcept
{
    if (systemHook_ != nullptr)
    {
        UnhookWinEvent(systemHook_);
        systemHook_ = nullptr;
    }
    if (objectHook_ != nullptr)
    {
        UnhookWinEvent(objectHook_);
        objectHook_ = nullptr;
    }
}

} // namespace tracing::platform
