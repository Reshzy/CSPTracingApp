#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/OverlaySurface.h"

#include <d3d11_1.h>

#include <cstring>
#include <cstdio>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif
#ifndef GET_POINTERID_WPARAM
#define GET_POINTERID_WPARAM(wParam) (LOWORD((wParam)))
#endif
#ifndef PT_PEN
#define PT_PEN 3
#endif

namespace tracing::graphics {
namespace {

constexpr wchar_t kOverlayClass[] = L"TracingAppOverlayWindow";
constexpr unsigned kDefaultWidth = 240;
constexpr unsigned kDefaultHeight = 160;
constexpr unsigned kMagentaWidth = 96;
constexpr unsigned kMagentaHeight = 40;
constexpr unsigned kMagentaX = 16;
constexpr unsigned kMagentaY = 24;
constexpr unsigned kYellowWidth = 32;
constexpr unsigned kYellowHeight = 48;
constexpr unsigned kYellowX = 16;
constexpr unsigned kYellowY = 64;
constexpr unsigned kMaxDimension = 16384;

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

std::wstring FormatHex32(unsigned long value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

OverlaySurface* SurfaceFromHwnd(HWND hwnd) noexcept
{
    return reinterpret_cast<OverlaySurface*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

bool ClipRect(D3D11_RECT& rect, unsigned destWidth, unsigned destHeight) noexcept
{
    if (rect.left >= static_cast<LONG>(destWidth) || rect.top >= static_cast<LONG>(destHeight))
    {
        return false;
    }
    if (rect.right > static_cast<LONG>(destWidth))
    {
        rect.right = static_cast<LONG>(destWidth);
    }
    if (rect.bottom > static_cast<LONG>(destHeight))
    {
        rect.bottom = static_cast<LONG>(destHeight);
    }
    return rect.right > rect.left && rect.bottom > rect.top;
}

std::uint64_t QueryCounter() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return static_cast<std::uint64_t>(value.QuadPart);
}

unsigned long CounterToMicroseconds(std::uint64_t start, std::uint64_t end) noexcept
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart <= 0 || end < start)
    {
        return 0;
    }
    return static_cast<unsigned long>(
        ((end - start) * 1000000ull) / static_cast<std::uint64_t>(frequency.QuadPart));
}

} // namespace

OverlaySurface::~OverlaySurface()
{
    Release();
}

bool OverlaySurface::Create(HWND controlWindow, DeviceResources& device, std::wstring& error)
{
    error.clear();
    Release();

    if (!device.IsReady() || device.Device() == nullptr || device.ImmediateContext() == nullptr)
    {
        error = L"Overlay surface needs a ready BGRA D3D11 device.";
        lastError_ = error;
        return false;
    }

    device_ = &device;
    controlWindow_ = controlWindow;

    HINSTANCE const instance = GetModuleHandleW(nullptr);
    if (!RegisterOverlayClass(instance, error) || !CreateOverlayWindow(instance, error))
    {
        lastError_ = error;
        Release();
        return false;
    }

    if (!CreateGpuSurfaces(kDefaultWidth, kDefaultHeight, error))
    {
        lastError_ = error;
        Release();
        return false;
    }

    contentReady_ = true;
    ApplyExcludeFromCapture();
    lastError_.clear();
    return true;
}

void OverlaySurface::Release()
{
    HideWindowOnly();
    contentReady_ = false;
    visible_ = false;
    ReleaseGpuSurfaces();
    ReleaseDib();
    if (hwnd_ != nullptr)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    device_ = nullptr;
    controlWindow_ = nullptr;
    affinity_ = {};
    lastVisibility_ = {};
    lastPlacement_ = {};
    lastPresentResult_ = S_OK;
    lastReadbackUs_ = 0;
    lastUlwUs_ = 0;
    emergencyHidden_ = false;
    testPatternActive_ = false;
    topmostWhileShown_ = false;
    mode_ = OverlayInteractionMode::Tracing;
    consumedMouseDown_ = 0;
    consumedWheel_ = 0;
    consumedPointerDown_ = 0;
}

void OverlaySurface::EmergencyHide()
{
    emergencyHidden_ = true;
    testPatternActive_ = false;
    HideWindowOnly();
    lastVisibility_.decision = OverlayVisibilityDecision::Hide;
    lastVisibility_.reason = OverlayVisibilityReason::EmergencyHidden;
}

void OverlaySurface::RequestTestPattern()
{
    RequestTestPatternAt(TestPatternPlacement());
}

void OverlaySurface::RequestTestPatternAt(OverlayPlacement const& placement)
{
    emergencyHidden_ = false;
    testPatternActive_ = true;
    std::wstring error;
    UpdatePlacementAndVisibility(placement, false, false, true, false, error);
    if (!error.empty())
    {
        lastError_ = error;
    }
}

void OverlaySurface::SetInteractionMode(OverlayInteractionMode mode)
{
    if (mode_ == mode)
    {
        return;
    }
    mode_ = mode;
    if (mode_ == OverlayInteractionMode::Tracing)
    {
        consumedMouseDown_ = 0;
        consumedWheel_ = 0;
        consumedPointerDown_ = 0;
    }
    ApplyHitTestStyles();
    if (visible_ && contentReady_)
    {
        std::wstring error;
        DrawMarker(error);
        if (!error.empty())
        {
            lastError_ = error;
        }
    }
}

void OverlaySurface::UpdatePlacementAndVisibility(
    OverlayPlacement const& placement,
    bool targetUsable,
    bool targetForeground,
    bool ownerControlForeground,
    bool hasTarget,
    std::wstring& error)
{
    error.clear();
    if (hasTarget)
    {
        testPatternActive_ = false;
        lastPlacement_ = placement;
    }
    else if (testPatternActive_ && placement.width > 0 && placement.height > 0)
    {
        lastPlacement_ = placement;
    }
    else if (!testPatternActive_)
    {
        lastPlacement_ = placement;
    }

    OverlayVisibilityInput input{};
    input.targetUsable =
        targetUsable && lastPlacement_.width > 0 && lastPlacement_.height > 0;
    input.targetForeground = targetForeground;
    input.ownerControlForeground = ownerControlForeground;
    input.affinityOk = AffinityIsReady(affinity_);
    input.emergencyHidden = emergencyHidden_;
    input.contentReady = contentReady_ && hwnd_ != nullptr && gpuTexture_ && stagingTexture_ &&
                         dibDc_ != nullptr && dibBits_ != nullptr;
    input.testPatternWithoutTarget = testPatternActive_ && !hasTarget;
    lastVisibility_ = DecideOverlayVisibility(input);

    if (lastVisibility_.decision != OverlayVisibilityDecision::Show)
    {
        HideWindowOnly();
        return;
    }

    unsigned width = static_cast<unsigned>(lastPlacement_.width);
    unsigned height = static_cast<unsigned>(lastPlacement_.height);
    if (!EnsureSurfaceSize(width, height, error) || !DrawMarker(error))
    {
        lastError_ = error;
        HideWindowOnly();
        lastVisibility_.decision = OverlayVisibilityDecision::Hide;
        lastVisibility_.reason = OverlayVisibilityReason::ContentNotReady;
        return;
    }

    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        lastPlacement_.x,
        lastPlacement_.y,
        lastPlacement_.width,
        lastPlacement_.height,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    topmostWhileShown_ = true;
    visible_ = true;
    lastError_.clear();
}

std::wstring OverlaySurface::FormatReport() const
{
    wchar_t hwndBuffer[32]{};
    swprintf_s(hwndBuffer, L"0x%p", static_cast<void*>(hwnd_));

    int const virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int const virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int const virtualW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int const virtualH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    std::wstring text = L"overlay hwnd=";
    text += hwndBuffer;
    text += L" visible=";
    text += visible_ ? L"yes" : L"no";
    text += L" contentReady=";
    text += contentReady_ ? L"yes" : L"no";
    text += L"\r\naffinity setCalled=";
    text += affinity_.setCalled ? L"yes" : L"no";
    text += L" set=";
    text += affinity_.setResult != 0 ? L"ok" : L"fail";
    text += L" win32=";
    text += std::to_wstring(affinity_.setLastError);
    text += L" readback=";
    text += FormatHex32(affinity_.readback);
    text += L" matchExclude=";
    text += affinity_.matchesExcludeFromCapture ? L"yes" : L"no";
    text += L"\r\nOBS verification=";
    text += FormatObsVerification(ObsVerification());
    text += L" (API success is not OBS proof)";
    text += L"\r\nvisibility=";
    text += FormatOverlayVisibilityDecision(lastVisibility_.decision);
    text += L" reason=";
    text += FormatOverlayVisibilityReason(lastVisibility_.reason);
    text += L" emergency=";
    text += emergencyHidden_ ? L"yes" : L"no";
    text += L" testPattern=";
    text += testPatternActive_ ? L"yes" : L"no";
    LONG_PTR const ex = CurrentExStyle();
    text += L"\r\ninteraction=";
    text += FormatOverlayInteractionMode(mode_);
    text += L" exstyle transparent=";
    text += (ex & WS_EX_TRANSPARENT) != 0 ? L"yes" : L"no";
    text += L" noactivate=";
    text += (ex & WS_EX_NOACTIVATE) != 0 ? L"yes" : L"no";
    text += L" noredirectionbitmap=";
    text += (ex & WS_EX_NOREDIRECTIONBITMAP) != 0 ? L"yes" : L"no";
    text += L" layered=";
    text += (ex & WS_EX_LAYERED) != 0 ? L"yes" : L"no";
    text += L"\r\nzOrder=";
    text += topmostWhileShown_ && visible_ ? L"topmost-while-shown" : L"not-topmost";
    text += L" consumed mouseDown=";
    text += std::to_wstring(consumedMouseDown_);
    text += L" wheel=";
    text += std::to_wstring(consumedWheel_);
    text += L" pointerDown=";
    text += std::to_wstring(consumedPointerDown_);
    text += L" (tracing should stay 0 if pass-through works)";
    text += L"\r\nplacement origin=(";
    text += std::to_wstring(lastPlacement_.x);
    text += L",";
    text += std::to_wstring(lastPlacement_.y);
    text += L") size=";
    text += std::to_wstring(lastPlacement_.width);
    text += L"x";
    text += std::to_wstring(lastPlacement_.height);
    text += L" (test pattern, not imported image; not canvas bounds)";
    text += L"\r\nvirtualScreen origin=(";
    text += std::to_wstring(virtualX);
    text += L",";
    text += std::to_wstring(virtualY);
    text += L") size=";
    text += std::to_wstring(virtualW);
    text += L"x";
    text += std::to_wstring(virtualH);
    text += L" present=";
    text += FormatHresult(lastPresentResult_);
    text += L" readbackUs=";
    text += std::to_wstring(lastReadbackUs_);
    text += L" ulwUs=";
    text += std::to_wstring(lastUlwUs_);
    if (!lastError_.empty())
    {
        text += L"\r\noverlay error: ";
        text += lastError_;
    }
    return text;
}

bool OverlaySurface::RegisterOverlayClass(HINSTANCE instance, std::wstring& error)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = OverlaySurface::WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kOverlayClass;
    ATOM const atom = RegisterClassExW(&windowClass);
    if (atom == 0)
    {
        unsigned long const code = GetLastError();
        if (code != ERROR_CLASS_ALREADY_EXISTS)
        {
            error = L"RegisterClassExW overlay failed (Win32 " + std::to_wstring(code) + L").";
            return false;
        }
    }
    return true;
}

bool OverlaySurface::CreateOverlayWindow(HINSTANCE instance, std::wstring& error)
{
    DWORD const exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
                          (OverlayUsesTransparentExStyle(mode_) ? WS_EX_TRANSPARENT : 0);
    hwnd_ = CreateWindowExW(
        exStyle,
        kOverlayClass,
        L"TracingAppOverlay",
        WS_POPUP,
        0,
        0,
        static_cast<int>(kDefaultWidth),
        static_cast<int>(kDefaultHeight),
        nullptr,
        nullptr,
        instance,
        this);
    if (hwnd_ == nullptr)
    {
        unsigned long const code = GetLastError();
        error = L"CreateWindowExW overlay failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    return ApplyHitTestStyles();
}

bool OverlaySurface::ApplyHitTestStyles()
{
    if (hwnd_ == nullptr)
    {
        return false;
    }

    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    ex |= WS_EX_LAYERED;
    if (OverlayUsesTransparentExStyle(mode_))
    {
        ex |= WS_EX_TRANSPARENT;
    }
    else
    {
        ex &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return true;
}

LONG_PTR OverlaySurface::CurrentExStyle() const noexcept
{
    if (hwnd_ == nullptr)
    {
        return 0;
    }
    return GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
}

LRESULT CALLBACK OverlaySurface::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* surface = static_cast<OverlaySurface*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(surface));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    auto* surface = SurfaceFromHwnd(hwnd);
    switch (message)
    {
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, message, wParam, lParam);
    case WM_NCHITTEST:
        return OverlayHitTestCode(
            surface != nullptr ? surface->mode_ : OverlayInteractionMode::Tracing);
    case WM_MOUSEACTIVATE:
        if (surface != nullptr && surface->mode_ == OverlayInteractionMode::Alignment)
        {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
        return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
        if (surface != nullptr)
        {
            ++surface->consumedMouseDown_;
        }
        return 0;
    case WM_POINTERDOWN:
        if (surface != nullptr)
        {
            ++surface->consumedPointerDown_;
            UINT32 const pointerId = GET_POINTERID_WPARAM(wParam);
            POINTER_INPUT_TYPE type{};
            if (GetPointerType(pointerId, &type) == FALSE || type != PT_PEN)
            {
                ++surface->consumedMouseDown_;
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (surface != nullptr)
        {
            ++surface->consumedWheel_;
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

bool OverlaySurface::CreateGpuSurfaces(unsigned width, unsigned height, std::wstring& error)
{
    return EnsureSurfaceSize(width, height, error);
}

bool OverlaySurface::ApplyExcludeFromCapture()
{
    affinity_ = {};
    affinity_.setCalled = true;
    SetLastError(0);
    BOOL const setResult = SetWindowDisplayAffinity(hwnd_, WDA_EXCLUDEFROMCAPTURE);
    affinity_.setLastError = GetLastError();
    affinity_.setResult = setResult != FALSE ? 1 : 0;

    DWORD readback = 0;
    BOOL const got = GetWindowDisplayAffinity(hwnd_, &readback);
    affinity_.readbackSucceeded = got != FALSE;
    affinity_.readback = readback;
    affinity_.matchesExcludeFromCapture =
        affinity_.readbackSucceeded && readback == WDA_EXCLUDEFROMCAPTURE;

    if (!AffinityIsReady(affinity_))
    {
        lastError_ = L"WDA_EXCLUDEFROMCAPTURE failed or readback mismatch; overlay stays hidden.";
        return false;
    }
    return true;
}

void OverlaySurface::ReleaseGpuSurfaces() noexcept
{
    rtv_.Reset();
    stagingTexture_.Reset();
    gpuTexture_.Reset();
    surfaceWidth_ = 0;
    surfaceHeight_ = 0;
}

void OverlaySurface::ReleaseDib() noexcept
{
    if (dibDc_ != nullptr && dibOld_ != nullptr)
    {
        SelectObject(dibDc_, dibOld_);
        dibOld_ = nullptr;
    }
    if (dibBitmap_ != nullptr)
    {
        DeleteObject(dibBitmap_);
        dibBitmap_ = nullptr;
    }
    if (dibDc_ != nullptr)
    {
        DeleteDC(dibDc_);
        dibDc_ = nullptr;
    }
    dibBits_ = nullptr;
}

bool OverlaySurface::EnsureDib(unsigned width, unsigned height, std::wstring& error)
{
    if (dibDc_ != nullptr && dibBits_ != nullptr && dibBitmap_ != nullptr && width == surfaceWidth_ &&
        height == surfaceHeight_)
    {
        return true;
    }

    ReleaseDib();

    HDC const screen = GetDC(nullptr);
    if (screen == nullptr)
    {
        error = L"GetDC for layered DIB failed (Win32 " + std::to_wstring(GetLastError()) + L").";
        return false;
    }
    dibDc_ = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (dibDc_ == nullptr)
    {
        error = L"CreateCompatibleDC failed (Win32 " + std::to_wstring(GetLastError()) + L").";
        return false;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    dibBitmap_ = CreateDIBSection(dibDc_, &info, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
    if (dibBitmap_ == nullptr || dibBits_ == nullptr)
    {
        unsigned long const code = GetLastError();
        ReleaseDib();
        error = L"CreateDIBSection failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    dibOld_ = SelectObject(dibDc_, dibBitmap_);
    return true;
}

bool OverlaySurface::EnsureSurfaceSize(unsigned width, unsigned height, std::wstring& error)
{
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension)
    {
        error = L"Overlay size is invalid.";
        return false;
    }
    if (gpuTexture_ && stagingTexture_ && rtv_ && dibDc_ != nullptr && dibBits_ != nullptr &&
        width == surfaceWidth_ && height == surfaceHeight_)
    {
        return true;
    }

    ReleaseGpuSurfaces();
    if (!EnsureDib(width, height, error))
    {
        return false;
    }

    if (!device_ || device_->Device() == nullptr)
    {
        error = L"Overlay GPU surfaces need a ready D3D11 device.";
        return false;
    }

    D3D11_TEXTURE2D_DESC gpu{};
    gpu.Width = width;
    gpu.Height = height;
    gpu.MipLevels = 1;
    gpu.ArraySize = 1;
    gpu.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    gpu.SampleDesc.Count = 1;
    gpu.Usage = D3D11_USAGE_DEFAULT;
    gpu.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT hr = device_->Device()->CreateTexture2D(&gpu, nullptr, &gpuTexture_);
    if (FAILED(hr) || !gpuTexture_)
    {
        error = L"CreateTexture2D overlay GPU target failed " + FormatHresult(hr) + L".";
        ReleaseGpuSurfaces();
        return false;
    }

    D3D11_TEXTURE2D_DESC staging = gpu;
    staging.Usage = D3D11_USAGE_STAGING;
    staging.BindFlags = 0;
    staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = device_->Device()->CreateTexture2D(&staging, nullptr, &stagingTexture_);
    if (FAILED(hr) || !stagingTexture_)
    {
        error = L"CreateTexture2D overlay staging failed " + FormatHresult(hr) + L".";
        ReleaseGpuSurfaces();
        return false;
    }

    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (!BindRenderTarget(error))
    {
        ReleaseGpuSurfaces();
        return false;
    }
    return true;
}

bool OverlaySurface::BindRenderTarget(std::wstring& error)
{
    rtv_.Reset();
    if (!device_ || !gpuTexture_)
    {
        error = L"BindRenderTarget skipped: GPU texture missing.";
        return false;
    }

    HRESULT const hr = device_->Device()->CreateRenderTargetView(gpuTexture_.Get(), nullptr, &rtv_);
    if (FAILED(hr) || !rtv_)
    {
        error = L"CreateRenderTargetView failed " + FormatHresult(hr) + L".";
        return false;
    }
    return true;
}

bool OverlaySurface::DrawMarker(std::wstring& error)
{
    if (!device_ || !rtv_ || !gpuTexture_)
    {
        error = L"Overlay draw skipped: resources missing.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
    HRESULT const qi = device_->ImmediateContext()->QueryInterface(IID_PPV_ARGS(&context1));
    if (FAILED(qi) || !context1)
    {
        error = L"ID3D11DeviceContext1 required to draw overlay marker " + FormatHresult(qi) + L".";
        return false;
    }

    ID3D11RenderTargetView* views[] = {rtv_.Get()};
    context1->OMSetRenderTargets(1, views, nullptr);
    // Tracing keeps a fully transparent clear so empty pixels are not hit-tested.
    // Alignment uses a barely-visible premultiplied alpha fill so the whole HWND
    // receives clicks (UpdateLayeredWindow ignores alpha=0 for hit-testing).
    if (mode_ == OverlayInteractionMode::Alignment)
    {
        float const alignClear[4] = {0.0f, 0.0f, 0.0f, 32.0f / 255.0f};
        context1->ClearRenderTargetView(rtv_.Get(), alignClear);
        D3D11_RECT strip{0, 0, static_cast<LONG>(surfaceWidth_), 8};
        if (ClipRect(strip, surfaceWidth_, surfaceHeight_))
        {
            float const cyan[4] = {0.0f, 1.0f, 1.0f, 1.0f};
            context1->ClearView(rtv_.Get(), cyan, &strip, 1);
        }
    }
    else
    {
        float const clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context1->ClearRenderTargetView(rtv_.Get(), clear);
    }

    D3D11_RECT magentaRect{
        static_cast<LONG>(kMagentaX),
        static_cast<LONG>(kMagentaY),
        static_cast<LONG>(kMagentaX + kMagentaWidth),
        static_cast<LONG>(kMagentaY + kMagentaHeight)};
    if (ClipRect(magentaRect, surfaceWidth_, surfaceHeight_))
    {
        float const magenta[4] = {1.0f, 0.0f, 1.0f, 1.0f};
        context1->ClearView(rtv_.Get(), magenta, &magentaRect, 1);
    }

    D3D11_RECT yellowRect{
        static_cast<LONG>(kYellowX),
        static_cast<LONG>(kYellowY),
        static_cast<LONG>(kYellowX + kYellowWidth),
        static_cast<LONG>(kYellowY + kYellowHeight)};
    if (ClipRect(yellowRect, surfaceWidth_, surfaceHeight_))
    {
        float const yellow[4] = {1.0f, 1.0f, 0.0f, 1.0f};
        context1->ClearView(rtv_.Get(), yellow, &yellowRect, 1);
    }

    context1->OMSetRenderTargets(0, nullptr, nullptr);
    return PresentLayered(error);
}

bool OverlaySurface::PresentLayered(std::wstring& error)
{
    if (!device_ || !gpuTexture_ || !stagingTexture_ || dibDc_ == nullptr || dibBits_ == nullptr ||
        hwnd_ == nullptr)
    {
        error = L"Layered present skipped: resources missing.";
        return false;
    }

    ID3D11DeviceContext* const context = device_->ImmediateContext();
    std::uint64_t const readbackStart = QueryCounter();
    context->CopyResource(stagingTexture_.Get(), gpuTexture_.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context->Map(stagingTexture_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        lastPresentResult_ = hr;
        error = L"Map overlay staging failed " + FormatHresult(hr) + L".";
        return false;
    }

    unsigned const rowBytes = surfaceWidth_ * 4u;
    auto* dest = static_cast<std::uint8_t*>(dibBits_);
    auto const* src = static_cast<std::uint8_t const*>(mapped.pData);
    for (unsigned y = 0; y < surfaceHeight_; ++y)
    {
        std::memcpy(dest + (static_cast<size_t>(y) * rowBytes), src + (static_cast<size_t>(y) * mapped.RowPitch), rowBytes);
    }
    context->Unmap(stagingTexture_.Get(), 0);
    lastReadbackUs_ = CounterToMicroseconds(readbackStart, QueryCounter());

    POINT destination{lastPlacement_.x, lastPlacement_.y};
    POINT source{0, 0};
    SIZE size{
        static_cast<LONG>(surfaceWidth_),
        static_cast<LONG>(surfaceHeight_)};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    std::uint64_t const ulwStart = QueryCounter();
    SetLastError(0);
    BOOL const updated = UpdateLayeredWindow(
        hwnd_,
        nullptr,
        &destination,
        &size,
        dibDc_,
        &source,
        0,
        &blend,
        ULW_ALPHA);
    lastUlwUs_ = CounterToMicroseconds(ulwStart, QueryCounter());
    if (updated == FALSE)
    {
        unsigned long const code = GetLastError();
        lastPresentResult_ = HRESULT_FROM_WIN32(code);
        error = L"UpdateLayeredWindow failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }

    lastPresentResult_ = S_OK;
    return true;
}

void OverlaySurface::HideWindowOnly()
{
    if (hwnd_ != nullptr)
    {
        SetWindowPos(
            hwnd_,
            HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    topmostWhileShown_ = false;
    visible_ = false;
}

OverlayPlacement OverlaySurface::TestPatternPlacement() const
{
    OverlayPlacement placement{};
    placement.width = static_cast<long>(kDefaultWidth);
    placement.height = static_cast<long>(kDefaultHeight);
    if (controlWindow_ != nullptr)
    {
        RECT rect{};
        if (GetWindowRect(controlWindow_, &rect) != FALSE)
        {
            placement.x = rect.left + 40;
            placement.y = rect.top + 40;
            return placement;
        }
    }
    placement.x = 100;
    placement.y = 100;
    return placement;
}

} // namespace tracing::graphics
