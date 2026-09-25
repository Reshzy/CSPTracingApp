#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif
#ifndef WM_POINTERUPDATE
#define WM_POINTERUPDATE 0x0245
#endif
#ifndef WM_POINTERWHEEL
#define WM_POINTERWHEEL 0x0248
#endif
#ifndef GET_POINTERID_WPARAM
#define GET_POINTERID_WPARAM(wParam) (LOWORD((wParam)))
#endif
#ifndef PT_PEN
#define PT_PEN 3
#endif

namespace {

constexpr wchar_t kProbeClass[] = L"TracingAppInputProbe";
constexpr wchar_t kProbeTitle[] = L"TracingApp InputProbe";
constexpr int kClientWidth = 640;
constexpr int kClientHeight = 480;
constexpr int kIdCounts = 2001;

struct ProbeState
{
    HWND window = nullptr;
    HWND counts = nullptr;
    unsigned mouseDown = 0;
    unsigned mouseUp = 0;
    unsigned wheel = 0;
    unsigned pointerPenDown = 0;
    unsigned pointerPenUpdate = 0;
    unsigned lastPenPressure = 0;
    bool lastPenPressureValid = false;
};

void RefreshCounts(ProbeState& state)
{
    bool const foreground = state.window != nullptr && GetForegroundWindow() == state.window;
    wchar_t text[512]{};
    swprintf_s(
        text,
        L"TracingApp InputProbe (other process)\r\n"
        L"mouseDown=%u mouseUp=%u wheel=%u\r\n"
        L"penDown=%u penUpdate=%u lastPressure=%u%s\r\n"
        L"foregroundSelf=%s",
        state.mouseDown,
        state.mouseUp,
        state.wheel,
        state.pointerPenDown,
        state.pointerPenUpdate,
        state.lastPenPressure,
        state.lastPenPressureValid ? L"" : L" (none)",
        foreground ? L"yes" : L"no");
    if (state.counts != nullptr)
    {
        SetWindowTextW(state.counts, text);
    }
    wchar_t title[256]{};
    swprintf_s(
        title,
        L"%s  down=%u wheel=%u pen=%u",
        kProbeTitle,
        state.mouseDown,
        state.wheel,
        state.pointerPenDown);
    if (state.window != nullptr)
    {
        SetWindowTextW(state.window, title);
    }
}

void NotePen(ProbeState& state, WPARAM wParam, bool isDown)
{
    UINT32 const pointerId = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE type{};
    if (GetPointerType(pointerId, &type) == FALSE || type != PT_PEN)
    {
        return;
    }
    if (isDown)
    {
        ++state.pointerPenDown;
    }
    else
    {
        ++state.pointerPenUpdate;
    }
    POINTER_PEN_INFO pen{};
    if (GetPointerPenInfo(pointerId, &pen) != FALSE)
    {
        state.lastPenPressure = pen.pressure;
        state.lastPenPressureValid = true;
    }
}

LRESULT CALLBACK ProbeWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<ProbeState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message)
    {
    case WM_CREATE:
    {
        auto* created = new ProbeState();
        created->window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        EnableMouseInPointer(TRUE);
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
        created->counts = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            16,
            16,
            600,
            200,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCounts)),
            instance,
            nullptr);
        RefreshCounts(*created);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (state != nullptr)
        {
            ++state->mouseDown;
            RefreshCounts(*state);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state != nullptr)
        {
            ++state->mouseUp;
            RefreshCounts(*state);
        }
        return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (state != nullptr)
        {
            ++state->wheel;
            RefreshCounts(*state);
        }
        return 0;
    case WM_POINTERDOWN:
        if (state != nullptr)
        {
            NotePen(*state, wParam, true);
            RefreshCounts(*state);
        }
        return 0;
    case WM_POINTERUPDATE:
        if (state != nullptr)
        {
            NotePen(*state, wParam, false);
            RefreshCounts(*state);
        }
        return 0;
    case WM_POINTERWHEEL:
        if (state != nullptr)
        {
            ++state->wheel;
            RefreshCounts(*state);
        }
        return 0;
    case WM_ERASEBKGND:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH brush = CreateSolidBrush(RGB(16, 96, 96));
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        return 1;
    }
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
}

bool RegisterProbeClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ProbeWndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kProbeClass;
    ATOM const atom = RegisterClassExW(&windowClass);
    if (atom != 0)
    {
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND CreateProbeWindow(HINSTANCE instance, bool visible)
{
    RECT wanted{0, 0, kClientWidth, kClientHeight};
    AdjustWindowRectEx(&wanted, WS_OVERLAPPEDWINDOW, FALSE, 0);
    HWND const hwnd = CreateWindowExW(
        0,
        kProbeClass,
        kProbeTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wanted.right - wanted.left,
        wanted.bottom - wanted.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (hwnd == nullptr)
    {
        return nullptr;
    }
    if (visible)
    {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

int RunSelfTest(HINSTANCE instance)
{
    if (!RegisterProbeClass(instance))
    {
        return 2;
    }
    HWND const hwnd = CreateProbeWindow(instance, false);
    if (hwnd == nullptr)
    {
        return 3;
    }
    auto* state = reinterpret_cast<ProbeState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr)
    {
        DestroyWindow(hwnd);
        return 4;
    }

    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
    SendMessageW(hwnd, WM_LBUTTONUP, 0, 0);
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);

    int exitCode = 0;
    if (state->mouseDown < 1 || state->mouseUp < 1 || state->wheel < 1)
    {
        exitCode = 1;
    }

    DestroyWindow(hwnd);
    MSG leftover{};
    while (PeekMessageW(&leftover, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
    }
    return exitCode;
}

bool WantsSelfTest()
{
    wchar_t const* command = GetCommandLineW();
    return command != nullptr && wcsstr(command, L"--self-test") != nullptr;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    if (WantsSelfTest())
    {
        return RunSelfTest(instance);
    }

    if (!RegisterProbeClass(instance))
    {
        return 1;
    }

    HWND const window = CreateProbeWindow(instance, false);
    if (window == nullptr)
    {
        return 1;
    }
    ShowWindow(window, showCommand == 0 ? SW_SHOW : showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
