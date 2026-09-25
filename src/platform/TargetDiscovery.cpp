#include "platform/TargetDiscovery.h"

#include <cwchar>
#include <cwctype>
#include <iterator>

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

wchar_t Fold(wchar_t value) noexcept
{
    return static_cast<wchar_t>(towlower(value));
}

bool EqualsIgnoreCase(std::wstring_view left, std::wstring_view right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        if (Fold(left[i]) != Fold(right[i]))
        {
            return false;
        }
    }
    return true;
}

std::wstring BaseName(std::wstring const& path)
{
    std::size_t const slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return path;
    }
    return path.substr(slash + 1);
}

std::uint64_t ToU64(FILETIME const& time) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

std::wstring KindLabel(CandidateKind kind)
{
    switch (kind)
    {
    case CandidateKind::LikelyPaint:
        return L"PAINT";
    case CandidateKind::LikelyLauncher:
        return L"LAUNCHER";
    case CandidateKind::OtherVisible:
        return L"OTHER";
    case CandidateKind::AccessFailed:
        return L"ACCESS";
    case CandidateKind::RejectedOwned:
        return L"OWNED";
    case CandidateKind::RejectedNotVisible:
        return L"HIDDEN";
    }
    return L"UNKNOWN";
}

bool QueryProcessIdentity(std::uint32_t pid, ProcessIdentity& identity, std::wstring& error)
{
    identity.pid = pid;
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
    identity.creationTime = ToU64(created);

    wchar_t path[32768]{};
    DWORD pathChars = static_cast<DWORD>(std::size(path));
    if (QueryFullProcessImageNameW(process.get(), 0, path, &pathChars) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = FormatAccessFailure(pid, code);
        return false;
    }
    identity.imagePath.assign(path, pathChars);
    identity.imageBaseName = BaseName(identity.imagePath);
    return true;
}

struct EnumContext
{
    std::vector<WindowCandidate>* candidates = nullptr;
    DWORD selfPid = 0;
};

BOOL CALLBACK EnumTopLevelProc(HWND hwnd, LPARAM lparam)
{
    auto* context = reinterpret_cast<EnumContext*>(lparam);
    if (context == nullptr || context->candidates == nullptr)
    {
        return FALSE;
    }
    if (!IsWindow(hwnd))
    {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == context->selfPid)
    {
        return TRUE;
    }

    WindowCandidate candidate{};
    candidate.hwnd = hwnd;
    candidate.visible = IsWindowVisible(hwnd) != FALSE;
    candidate.hasOwner = GetWindow(hwnd, GW_OWNER) != nullptr;

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
    candidate.title = title;

    wchar_t className[256]{};
    GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
    candidate.className = className;

    bool const queried = QueryProcessIdentity(pid, candidate.process, candidate.accessError);
    if (!queried)
    {
        candidate.process.pid = pid;
    }
    candidate.kind = ClassifyCandidate(
        candidate.process.imageBaseName,
        candidate.visible,
        candidate.hasOwner,
        queried);
    context->candidates->push_back(std::move(candidate));
    return TRUE;
}

} // namespace

bool IsLikelyCspPaintExecutable(std::wstring_view imageBaseName)
{
    return EqualsIgnoreCase(imageBaseName, L"CLIPStudioPaint.exe");
}

bool IsLikelyCspLauncherExecutable(std::wstring_view imageBaseName)
{
    return EqualsIgnoreCase(imageBaseName, L"CLIPStudio.exe");
}

CandidateKind ClassifyCandidate(
    std::wstring_view imageBaseName,
    bool visible,
    bool hasOwner,
    bool processQuerySucceeded)
{
    if (!visible)
    {
        return CandidateKind::RejectedNotVisible;
    }
    if (hasOwner)
    {
        return CandidateKind::RejectedOwned;
    }
    if (!processQuerySucceeded)
    {
        return CandidateKind::AccessFailed;
    }
    if (IsLikelyCspPaintExecutable(imageBaseName))
    {
        return CandidateKind::LikelyPaint;
    }
    if (IsLikelyCspLauncherExecutable(imageBaseName))
    {
        return CandidateKind::LikelyLauncher;
    }
    return CandidateKind::OtherVisible;
}

IdentityCheck CheckIdentity(
    std::uint32_t storedPid,
    std::uint64_t storedCreationTime,
    bool hwndAlive,
    bool observedProcessKnown,
    std::uint32_t observedPid,
    std::uint64_t observedCreationTime)
{
    if (!hwndAlive)
    {
        return IdentityCheck::WindowDead;
    }
    if (!observedProcessKnown)
    {
        return IdentityCheck::AccessFailed;
    }
    if (storedPid != observedPid || storedCreationTime != observedCreationTime)
    {
        return IdentityCheck::HandleReused;
    }
    return IdentityCheck::Match;
}

TargetSelection ResolveSelection(
    std::span<WindowCandidate const> candidates,
    std::optional<std::size_t> userIndex,
    std::uint64_t sessionGeneration)
{
    TargetSelection result{};

    auto makeIdentity = [&](WindowCandidate const& candidate) {
        TargetIdentity identity{};
        identity.hwnd = candidate.hwnd;
        identity.process = candidate.process;
        identity.sessionGeneration = sessionGeneration;
        return identity;
    };

    if (userIndex.has_value())
    {
        if (*userIndex >= candidates.size())
        {
            result.status = SelectionStatus::InvalidIndex;
            result.message = L"Selection index is out of range. Refresh the list and choose again.";
            return result;
        }

        WindowCandidate const& candidate = candidates[*userIndex];
        if (!candidate.visible || candidate.kind == CandidateKind::RejectedNotVisible)
        {
            result.status = SelectionStatus::RejectedDead;
            result.message = L"That window is no longer a visible top-level target.";
            return result;
        }
        if (candidate.kind == CandidateKind::AccessFailed)
        {
            result.status = SelectionStatus::AccessFailed;
            result.message = candidate.accessError.empty()
                                 ? FormatAccessFailure(candidate.process.pid, ERROR_ACCESS_DENIED)
                                 : candidate.accessError;
            return result;
        }
        if (candidate.kind == CandidateKind::LikelyLauncher)
        {
            result.status = SelectionStatus::RejectedLauncher;
            result.message =
                L"That window belongs to CLIPStudio.exe (launcher), not CLIPStudioPaint.exe. "
                L"Select the painting window.";
            return result;
        }
        if (candidate.kind != CandidateKind::LikelyPaint)
        {
            result.status = SelectionStatus::RejectedOther;
            result.message =
                L"That window is not CLIPStudioPaint.exe. Process identity is " +
                (candidate.process.imageBaseName.empty() ? std::wstring(L"(unknown)")
                                                         : candidate.process.imageBaseName) +
                L".";
            return result;
        }

        result.status = SelectionStatus::Selected;
        result.identity = makeIdentity(candidate);
        result.message = L"Selected CLIPStudioPaint.exe PID " +
                         std::to_wstring(candidate.process.pid) + L", generation " +
                         std::to_wstring(sessionGeneration) + L".";
        return result;
    }

    std::vector<std::size_t> paintIndices;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        if (candidates[i].kind == CandidateKind::LikelyPaint)
        {
            paintIndices.push_back(i);
        }
    }

    if (paintIndices.empty())
    {
        result.status = SelectionStatus::NoPaintCandidate;
        result.message =
            L"No visible CLIPStudioPaint.exe top-level window found. Launch a painting document "
            L"and refresh. The launcher (CLIPStudio.exe) is not selected automatically.";
        return result;
    }
    if (paintIndices.size() > 1)
    {
        result.status = SelectionStatus::Ambiguous;
        result.message =
            L"Multiple CLIPStudioPaint.exe windows are visible. Select one explicitly so a "
            L"launcher or second instance cannot replace the target.";
        return result;
    }

    WindowCandidate const& onlyPaint = candidates[paintIndices.front()];
    result.status = SelectionStatus::Selected;
    result.identity = makeIdentity(onlyPaint);
    result.message = L"Selected the only CLIPStudioPaint.exe window, PID " +
                     std::to_wstring(onlyPaint.process.pid) + L", generation " +
                     std::to_wstring(sessionGeneration) + L".";
    return result;
}

std::wstring FormatAccessFailure(std::uint32_t pid, unsigned long win32Error)
{
    std::wstring message = L"Could not query process identity for PID " + std::to_wstring(pid) +
                           L" (Win32 " + std::to_wstring(win32Error) + L"). ";
    if (win32Error == ERROR_ACCESS_DENIED)
    {
        message +=
            L"Use the same user account that launched CLIP STUDIO PAINT. Do not run TracingApp "
            L"elevated unless that process is also elevated. Admin rights are not the default fix.";
    }
    else if (win32Error == ERROR_INVALID_PARAMETER)
    {
        message += L"The process is no longer running.";
    }
    else
    {
        message += L"Retry after the window is visible; this app does not inject into the target.";
    }
    return message;
}

std::wstring FormatCandidateLine(WindowCandidate const& candidate)
{
    std::wstring line = L"[" + KindLabel(candidate.kind) + L"] PID " +
                        std::to_wstring(candidate.process.pid);
    if (!candidate.process.imageBaseName.empty())
    {
        line += L" ";
        line += candidate.process.imageBaseName;
    }
    if (!candidate.title.empty())
    {
        line += L" | ";
        line += candidate.title;
    }
    if (!candidate.accessError.empty() && candidate.kind == CandidateKind::AccessFailed)
    {
        line += L" | ";
        line += candidate.accessError;
    }
    return line;
}

bool EnumerateTopLevelCandidates(std::vector<WindowCandidate>& candidates, std::wstring& error)
{
    candidates.clear();
    error.clear();
    EnumContext context{};
    context.candidates = &candidates;
    context.selfPid = GetCurrentProcessId();
    if (EnumWindows(EnumTopLevelProc, reinterpret_cast<LPARAM>(&context)) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = L"EnumWindows failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    return true;
}

} // namespace tracing::platform
