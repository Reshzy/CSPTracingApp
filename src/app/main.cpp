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

#include "platform/TargetDiscovery.h"

namespace {
constexpr int kDefaultWidth = 720;
constexpr int kDefaultHeight = 520;
constexpr wchar_t kWindowClass[] = L"TracingAppControlWindow";
constexpr wchar_t kWindowTitle[] = L"TracingApp";
constexpr int kIdList = 1001;
constexpr int kIdRefresh = 1002;
constexpr int kIdSelect = 1003;
constexpr int kIdStatus = 1004;

struct ControlState
{
    HWND list = nullptr;
    HWND status = nullptr;
    std::vector<tracing::platform::WindowCandidate> candidates;
    std::optional<tracing::platform::TargetIdentity> selected;
    std::uint64_t nextGeneration = 1;
};

void SetStatus(ControlState& state, std::wstring const& text)
{
    if (state.status != nullptr)
    {
        SetWindowTextW(state.status, text.c_str());
    }
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
        SetStatus(state, error);
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
            state.selected.reset();
            SetStatus(
                state,
                L"Previous target is gone. HWND is no longer valid; select a painting window again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::HandleReused)
        {
            state.selected.reset();
            SetStatus(
                state,
                L"Previous HWND was reused by another process. Target cleared so a launcher or "
                L"second instance cannot silently replace it. Select again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::AccessFailed)
        {
            SetStatus(
                state,
                accessError.empty()
                    ? tracing::platform::FormatAccessFailure(stored.process.pid, ERROR_ACCESS_DENIED)
                    : accessError);
            return;
        }
    }

    SetStatus(
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
    SetStatus(state, selection.message);
    if (selection.status == tracing::platform::SelectionStatus::Selected)
    {
        state.selected = selection.identity;
        ++state.nextGeneration;
    }
}

LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<ControlState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        auto* created = new ControlState();
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
            300,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdList)),
            instance,
            nullptr);
        created->status = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            12,
            368,
            680,
            100,
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
            324,
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
            324,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSelect)),
            instance,
            nullptr);
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
            if (id == kIdList && code == LBN_DBLCLK)
            {
                SelectFromUi(*state);
                return 0;
            }
        }
        break;
    case WM_DESTROY:
        if (state != nullptr)
        {
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
