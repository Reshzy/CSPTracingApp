#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

bool IsValidControlViewport(int width, int height) noexcept
{
    return width > 0 && height > 0;
}

#ifndef TRACING_APP_TESTING

#include "graphics/DeviceResources.h"
#include "graphics/OverlaySurface.h"
#include "platform/TargetDiscovery.h"
#include "platform/TargetGeometry.h"

namespace {
constexpr int kDefaultWidth = 720;
constexpr int kDefaultHeight = 760;
constexpr wchar_t kWindowClass[] = L"TracingAppControlWindow";
constexpr wchar_t kWindowTitle[] = L"TracingApp";
constexpr int kIdList = 1001;
constexpr int kIdRefresh = 1002;
constexpr int kIdSelect = 1003;
constexpr int kIdStatus = 1004;
constexpr int kIdEmergencyHide = 1005;
constexpr int kIdShowMarker = 1006;
constexpr UINT kMsgGeometry = WM_APP + 1;
constexpr UINT_PTR kTimerGeometry = 1;
constexpr UINT kGeometryPollMs = 250;

struct ControlState
{
    HWND control = nullptr;
    HWND list = nullptr;
    HWND status = nullptr;
    std::vector<tracing::platform::WindowCandidate> candidates;
    std::optional<tracing::platform::TargetIdentity> selected;
    tracing::platform::TargetLifecycleWatcher watcher;
    tracing::platform::GeometrySnapshot lastGeometry;
    tracing::graphics::DeviceResources device;
    tracing::graphics::OverlaySurface overlay;
    std::uint64_t nextGeneration = 1;
};

std::wstring ControlDpiLine(HWND hwnd)
{
    unsigned const dpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : 0;
    return L"control DPI=" + std::to_wstring(dpi) +
           L" (physical, PerMonitorV2; not canvas bounds)\r\n";
}

std::wstring StatusHeader(ControlState const& state)
{
    return ControlDpiLine(state.control) + state.device.FormatReport() + L"\r\n" +
           state.overlay.FormatReport() + L"\r\n";
}

void SetStatus(ControlState& state, std::wstring const& text)
{
    if (state.status != nullptr)
    {
        SetWindowTextW(state.status, text.c_str());
    }
}

void StopWatching(ControlState& state)
{
    state.watcher.Detach();
    if (state.control != nullptr)
    {
        KillTimer(state.control, kTimerGeometry);
    }
    state.lastGeometry = {};
}

void SyncOverlayFromState(ControlState& state)
{
    tracing::graphics::OverlayPlacement placement{};
    bool targetUsable = false;
    bool targetForeground = false;
    bool const hasTarget = state.selected.has_value();
    bool const controlForeground =
        state.control != nullptr && GetForegroundWindow() == state.control;

    if (hasTarget)
    {
        tracing::platform::GeometrySnapshot const& geometry = state.lastGeometry;
        tracing::platform::OverlayEligibility const eligibility =
            tracing::platform::ClassifyOverlayEligibility(geometry);
        targetUsable = eligibility == tracing::platform::OverlayEligibility::Eligible ||
                       eligibility == tracing::platform::OverlayEligibility::NotForeground;
        targetForeground = geometry.targetForeground;
        placement.x = geometry.clientPhysical.x;
        placement.y = geometry.clientPhysical.y;
        placement.width = geometry.clientPhysical.width;
        placement.height = geometry.clientPhysical.height;
    }
    else if (state.overlay.TestPatternActive())
    {
        placement = state.overlay.LastPlacement();
    }

    std::wstring error;
    state.overlay.UpdatePlacementAndVisibility(
        placement,
        targetUsable,
        targetForeground,
        controlForeground,
        hasTarget,
        error);
}

void SetStatusWithOverlay(ControlState& state, std::wstring const& body)
{
    SyncOverlayFromState(state);
    if (body.empty())
    {
        SetStatus(state, StatusHeader(state));
        return;
    }
    SetStatus(state, StatusHeader(state) + body);
}

void RefreshGeometryDisplay(ControlState& state, std::wstring const& extra)
{
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, extra);
        return;
    }

    tracing::platform::GeometrySnapshot sampled{};
    std::wstring error;
    bool const ok = tracing::platform::QueryPhysicalGeometry(*state.selected, sampled, error);
    if (!ok)
    {
        if (sampled.identity == tracing::platform::IdentityCheck::WindowDead ||
            sampled.identity == tracing::platform::IdentityCheck::HandleReused)
        {
            StopWatching(state);
            state.selected.reset();
            std::wstring body = error;
            if (!extra.empty())
            {
                body += L"\r\n";
                body += extra;
            }
            SetStatusWithOverlay(state, body);
            return;
        }
        std::wstring body = error + L"\r\n" + tracing::platform::FormatGeometryReport(sampled);
        if (!extra.empty())
        {
            body += L"\r\n";
            body += extra;
        }
        SetStatusWithOverlay(state, body);
        return;
    }

    sampled.geometryGeneration =
        tracing::platform::NextGeometryGeneration(state.lastGeometry, sampled);
    state.lastGeometry = sampled;
    std::wstring body = tracing::platform::FormatGeometryReport(sampled);
    if (!extra.empty())
    {
        body += L"\r\n";
        body += extra;
    }
    SetStatusWithOverlay(state, body);
}

bool StartWatching(ControlState& state, std::wstring& hookError)
{
    hookError.clear();
    if (!state.selected.has_value() || state.control == nullptr)
    {
        hookError = L"Cannot watch geometry without a selected target.";
        return false;
    }

    StopWatching(state);
    bool const hooked = state.watcher.Attach(
        state.control,
        state.selected->hwnd,
        kMsgGeometry,
        hookError);
    SetTimer(state.control, kTimerGeometry, kGeometryPollMs, nullptr);
    return hooked;
}

void RefillList(ControlState& state)
{
    SendMessageW(state.list, LB_RESETCONTENT, 0, 0);
    for (std::size_t i = 0; i < state.candidates.size(); ++i)
    {
        tracing::platform::WindowCandidate const& candidate = state.candidates[i];
        if (candidate.kind == tracing::platform::CandidateKind::RejectedNotVisible)
        {
            continue;
        }
        std::wstring const line = tracing::platform::FormatCandidateLine(candidate);
        LRESULT const index = SendMessageW(
            state.list,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(line.c_str()));
        if (index != LB_ERR)
        {
            SendMessageW(state.list, LB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(i));
        }
    }
}

void RefreshCandidates(ControlState& state)
{
    std::wstring error;
    if (!tracing::platform::EnumerateTopLevelCandidates(state.candidates, error))
    {
        SetStatusWithOverlay(state, error);
        return;
    }
    RefillList(state);

    if (state.selected.has_value())
    {
        tracing::platform::TargetIdentity const& stored = *state.selected;
        bool const hwndAlive = IsWindow(stored.hwnd) != FALSE;
        tracing::platform::ProcessIdentity observed{};
        std::wstring accessError;
        bool observedKnown = false;
        if (hwndAlive)
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(stored.hwnd, &pid);
            observed.pid = pid;
            for (tracing::platform::WindowCandidate const& candidate : state.candidates)
            {
                if (candidate.hwnd == stored.hwnd)
                {
                    observed = candidate.process;
                    observedKnown = candidate.kind != tracing::platform::CandidateKind::AccessFailed &&
                                    candidate.process.creationTime != 0;
                    accessError = candidate.accessError;
                    break;
                }
            }
        }

        tracing::platform::IdentityCheck const check = tracing::platform::CheckIdentity(
            stored.process.pid,
            stored.process.creationTime,
            hwndAlive,
            observedKnown,
            observed.pid,
            observed.creationTime);
        if (check == tracing::platform::IdentityCheck::WindowDead)
        {
            StopWatching(state);
            state.selected.reset();
            SetStatusWithOverlay(
                state,
                L"Previous target is gone. HWND is no longer valid; geometry invalidated. "
                L"Select a painting window again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::HandleReused)
        {
            StopWatching(state);
            state.selected.reset();
            SetStatusWithOverlay(
                state,
                L"Previous HWND was reused by another process. Target and geometry cleared so "
                L"a launcher or second instance cannot silently replace it. Select again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::AccessFailed)
        {
            SetStatusWithOverlay(
                state,
                accessError.empty()
                    ? tracing::platform::FormatAccessFailure(
                          stored.process.pid, ERROR_ACCESS_DENIED)
                    : accessError);
            return;
        }

        RefreshGeometryDisplay(state, L"refreshed candidate list");
        return;
    }

    SetStatusWithOverlay(
        state,
        L"Refreshed top-level windows. Select a PAINT row (CLIPStudioPaint.exe), then Select. "
        L"Launcher rows are never chosen automatically.");
}

void SelectFromUi(ControlState& state)
{
    std::optional<std::size_t> userIndex;
    LRESULT const sel = SendMessageW(state.list, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR)
    {
        LRESULT const data = SendMessageW(state.list, LB_GETITEMDATA, static_cast<WPARAM>(sel), 0);
        if (data != LB_ERR)
        {
            userIndex = static_cast<std::size_t>(data);
        }
    }

    tracing::platform::TargetSelection const selection = tracing::platform::ResolveSelection(
        state.candidates,
        userIndex,
        state.nextGeneration);
    if (selection.status != tracing::platform::SelectionStatus::Selected)
    {
        SetStatusWithOverlay(state, selection.message);
        return;
    }

    StopWatching(state);
    state.selected = selection.identity;
    ++state.nextGeneration;

    std::wstring hookError;
    bool const hooked = StartWatching(state, hookError);
    std::wstring extra = selection.message;
    if (!hooked)
    {
        extra += L"\r\n";
        extra += hookError;
    }
    RefreshGeometryDisplay(state, extra);
}

void OnEmergencyHide(ControlState& state)
{
    state.overlay.EmergencyHide();
    SetStatusWithOverlay(
        state,
        L"Emergency hide latched. Overlay stays hidden until Show test marker.");
}

void OnShowTestMarker(ControlState& state)
{
    state.overlay.ClearEmergencyHide();
    if (!state.selected.has_value())
    {
        state.overlay.RequestTestPattern();
        SetStatusWithOverlay(
            state,
            L"Show test marker: test-pattern surface (not imported image).");
        return;
    }
    state.overlay.ClearTestPattern();
    RefreshGeometryDisplay(
        state,
        L"Show test marker: using selected target client physical bounds.");
}

LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<ControlState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        auto* created = new ControlState();
        created->control = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
        created->list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            12,
            12,
            680,
            200,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdList)),
            instance,
            nullptr);
        created->status = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            256,
            680,
            460,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatus)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Refresh",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            220,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRefresh)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Select",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            124,
            220,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSelect)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Emergency Hide",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            236,
            220,
            140,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEmergencyHide)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Show test marker",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            384,
            220,
            150,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShowMarker)),
            instance,
            nullptr);
        std::wstring deviceError;
        if (!created->device.Create(deviceError))
        {
            OutputDebugStringW(deviceError.c_str());
            OutputDebugStringW(L"\r\n");
        }
        else
        {
            std::wstring overlayError;
            if (!created->overlay.Create(hwnd, created->device, overlayError))
            {
                OutputDebugStringW(overlayError.c_str());
                OutputDebugStringW(L"\r\n");
            }
        }
        RefreshCandidates(*created);
        return 0;
    }
    case WM_COMMAND:
        if (state != nullptr)
        {
            int const id = LOWORD(wParam);
            int const code = HIWORD(wParam);
            if (id == kIdRefresh && code == BN_CLICKED)
            {
                RefreshCandidates(*state);
                return 0;
            }
            if (id == kIdSelect && code == BN_CLICKED)
            {
                SelectFromUi(*state);
                return 0;
            }
            if (id == kIdEmergencyHide && code == BN_CLICKED)
            {
                OnEmergencyHide(*state);
                return 0;
            }
            if (id == kIdShowMarker && code == BN_CLICKED)
            {
                OnShowTestMarker(*state);
                return 0;
            }
            if (id == kIdList && code == LBN_DBLCLK)
            {
                SelectFromUi(*state);
                return 0;
            }
        }
        break;
    case kMsgGeometry:
        if (state != nullptr && state->selected.has_value())
        {
            RefreshGeometryDisplay(*state, L"WinEvent");
            return 0;
        }
        break;
    case WM_TIMER:
        if (state != nullptr && wParam == kTimerGeometry && state->selected.has_value())
        {
            RefreshGeometryDisplay(*state, L"");
            return 0;
        }
        break;
    case WM_DPICHANGED:
        if (state != nullptr)
        {
            RefreshGeometryDisplay(*state, L"control DPI changed");
        }
        break;
    case WM_DESTROY:
        if (state != nullptr)
        {
            StopWatching(*state);
            state->overlay.Release();
            state->device.Release();
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    if (!IsValidControlViewport(kDefaultWidth, kDefaultHeight))
    {
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ControlWndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    HWND const window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kDefaultWidth,
        kDefaultHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

#endif
