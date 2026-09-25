#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/OverlaySurface.h"

#include <d3d11_1.h>
#include <dwmapi.h>

#include <algorithm>
#include <cstdio>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
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
constexpr unsigned kMarkerOriginX = 24;
constexpr unsigned kMarkerOriginY = 32;
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

void FillSwapDesc(DXGI_SWAP_CHAIN_DESC1& desc, unsigned width, unsigned height, DXGI_ALPHA_MODE alpha)
{
    desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = alpha;
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

    if (!CreateDxgiFactory(error) || !ApplyLayeredRedirection(error) ||
        !EnsurePresentSize(kDefaultWidth, kDefaultHeight, error))
    {
        lastError_ = error;
        Release();
        return false;
    }

    contentReady_ = true;
    if (ApplyActiveAffinity())
    {
        lastError_.clear();
    }
    return true;
}

void OverlaySurface::Release()
{
    HideWindowOnly();
    contentReady_ = false;
    visible_ = false;
    ReleaseSwapChain();
    factory_.Reset();
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
    alphaMode_ = OverlayPresentAlphaMode::Unspecified;
    lastPresentResult_ = S_OK;
    premulCreateHr_ = S_OK;
    ignoreCreateHr_ = S_OK;
    lastPresentUs_ = 0;
    emergencyHidden_ = false;
    testPatternActive_ = false;
    topmostWhileShown_ = false;
    dwmExtendedFrame_ = false;
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
    input.affinityOk = AffinityIsReadyForMode(affinity_, affinityMode_);
    input.emergencyHidden = emergencyHidden_;
    input.contentReady = contentReady_ && hwnd_ != nullptr && swapChain_ && rtv_;
    input.testPatternWithoutTarget = testPatternActive_ && !hasTarget;
    lastVisibility_ = DecideOverlayVisibility(input);

    if (lastVisibility_.decision != OverlayVisibilityDecision::Show)
    {
        HideWindowOnly();
        return;
    }

    unsigned width = static_cast<unsigned>(lastPlacement_.width);
    unsigned height = static_cast<unsigned>(lastPlacement_.height);
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        lastPlacement_.x,
        lastPlacement_.y,
        lastPlacement_.width,
        lastPlacement_.height,
        SWP_NOACTIVATE);
    topmostWhileShown_ = true;
    if (!EnsurePresentSize(width, height, error) || !DrawMarker(error))
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
    text += L" matchNone=";
    text += affinity_.matchesNone ? L"yes" : L"no";
    text += L" mode=";
    text += FormatOverlayAffinityMode(affinityMode_);
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
    text += L"\r\npresentPath=dxgi-hwnd (not ulw, not dcomp) alphaMode=";
    text += FormatOverlayPresentAlphaMode(alphaMode_);
    text += L" premulCreateHr=";
    text += FormatHresult(premulCreateHr_);
    text += L" ignoreCreateHr=";
    text += FormatHresult(ignoreCreateHr_);
    text += L" dwmExtendFrame=";
    text += dwmExtendedFrame_ ? L"yes" : L"no";
    text += L" present=";
    text += FormatHresult(lastPresentResult_);
    text += L" presentUs=";
    text += std::to_wstring(lastPresentUs_);
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

bool OverlaySurface::ApplyLayeredRedirection(std::wstring& error)
{
    if (hwnd_ == nullptr)
    {
        error = L"SetLayeredWindowAttributes skipped: overlay HWND is missing.";
        return false;
    }
    SetLastError(0);
    if (SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA) == FALSE)
    {
        unsigned long const code = GetLastError();
        error = L"SetLayeredWindowAttributes(LWA_ALPHA) failed (Win32 " + std::to_wstring(code) +
                L").";
        return false;
    }
    return true;
}

bool OverlaySurface::ApplyDwmExtendedFrame(std::wstring& error)
{
    if (hwnd_ == nullptr)
    {
        error = L"DwmExtendFrameIntoClientArea skipped: overlay HWND is missing.";
        return false;
    }
    MARGINS const margins{-1, -1, -1, -1};
    HRESULT const hr = DwmExtendFrameIntoClientArea(hwnd_, &margins);
    if (FAILED(hr))
    {
        error = L"DwmExtendFrameIntoClientArea failed " + FormatHresult(hr) + L".";
        dwmExtendedFrame_ = false;
        return false;
    }
    dwmExtendedFrame_ = true;
    return true;
}

bool OverlaySurface::CreateDxgiFactory(std::wstring& error)
{
    if (!device_ || device_->Device() == nullptr)
    {
        error = L"Overlay DXGI factory needs a ready D3D11 device.";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device_->Device()->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr) || !dxgiDevice)
    {
        error = L"QueryInterface IDXGIDevice overlay failed " + FormatHresult(hr) + L".";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr) || !adapter)
    {
        error = L"IDXGIDevice::GetAdapter overlay failed " + FormatHresult(hr) + L".";
        return false;
    }

    factory_.Reset();
    hr = adapter->GetParent(IID_PPV_ARGS(&factory_));
    if (FAILED(hr) || !factory_)
    {
        error = L"IDXGIAdapter::GetParent IDXGIFactory2 overlay failed " + FormatHresult(hr) + L".";
        return false;
    }
    return true;
}

bool OverlaySurface::ApplyHitTestStyles()
{
    if (hwnd_ == nullptr)
    {
        return false;
    }

    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    ex &= ~static_cast<LONG_PTR>(WS_EX_NOREDIRECTIONBITMAP);
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
    std::wstring layeredError;
    ApplyLayeredRedirection(layeredError);
    if (!layeredError.empty())
    {
        lastError_ = layeredError;
    }
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

bool OverlaySurface::SetAffinityMode(OverlayAffinityMode mode, std::wstring& error)
{
    error.clear();
    affinityMode_ = mode;
    if (hwnd_ == nullptr)
    {
        error = L"SetAffinityMode skipped: overlay HWND is missing.";
        lastError_ = error;
        return false;
    }
    if (!ApplyActiveAffinity())
    {
        error = lastError_;
        return false;
    }
    lastError_.clear();
    return true;
}

bool OverlaySurface::ApplyActiveAffinity()
{
    if (affinityMode_ == OverlayAffinityMode::TemporaryNonePositiveControl)
    {
        return ApplyNonePositiveControl();
    }
    return ApplyExcludeFromCapture();
}

bool OverlaySurface::ApplyExcludeFromCapture()
{
    return ApplyDisplayAffinity(WDA_EXCLUDEFROMCAPTURE);
}

bool OverlaySurface::ApplyNonePositiveControl()
{
    return ApplyDisplayAffinity(WDA_NONE);
}

bool OverlaySurface::ApplyDisplayAffinity(unsigned long affinity)
{
    if (hwnd_ == nullptr)
    {
        lastError_ = L"ApplyDisplayAffinity skipped: overlay HWND is missing.";
        return false;
    }

    affinity_ = {};
    affinity_.setCalled = true;
    SetLastError(0);
    BOOL const setResult = SetWindowDisplayAffinity(hwnd_, affinity);
    affinity_.setLastError = GetLastError();
    affinity_.setResult = setResult != FALSE ? 1 : 0;

    DWORD readback = 0;
    BOOL const got = GetWindowDisplayAffinity(hwnd_, &readback);
    affinity_.readbackSucceeded = got != FALSE;
    affinity_.readback = readback;
    affinity_.matchesExcludeFromCapture =
        affinity_.readbackSucceeded && readback == WDA_EXCLUDEFROMCAPTURE;
    affinity_.matchesNone = affinity_.readbackSucceeded && readback == WDA_NONE;

    if (!AffinityIsReadyForMode(affinity_, affinityMode_))
    {
        if (affinityMode_ == OverlayAffinityMode::TemporaryNonePositiveControl)
        {
            lastError_ =
                L"WDA_NONE positive-control failed or readback mismatch; overlay stays hidden.";
        }
        else
        {
            lastError_ =
                L"WDA_EXCLUDEFROMCAPTURE failed or readback mismatch; overlay stays hidden.";
        }
        return false;
    }
    return true;
}

void OverlaySurface::UnbindContextTargets() noexcept
{
    if (device_ == nullptr || device_->ImmediateContext() == nullptr)
    {
        return;
    }
    ID3D11RenderTargetView* none[] = {nullptr};
    device_->ImmediateContext()->OMSetRenderTargets(0, none, nullptr);
}

void OverlaySurface::ReleaseSwapChain() noexcept
{
    UnbindContextTargets();
    rtv_.Reset();
    swapChain_.Reset();
    surfaceWidth_ = 0;
    surfaceHeight_ = 0;
}

bool OverlaySurface::EnsurePresentSize(unsigned width, unsigned height, std::wstring& error)
{
    return EnsureSwapChain(width, height, error);
}

bool OverlaySurface::EnsureSwapChain(unsigned width, unsigned height, std::wstring& error)
{
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension)
    {
        error = L"Overlay size is invalid.";
        return false;
    }
    if (swapChain_ && rtv_ && width == surfaceWidth_ && height == surfaceHeight_)
    {
        return true;
    }

    if (!device_ || device_->Device() == nullptr || !factory_ || hwnd_ == nullptr)
    {
        error = L"Overlay swap chain needs a ready D3D11 device, factory, and HWND.";
        return false;
    }

    UnbindContextTargets();
    rtv_.Reset();

    if (!swapChain_)
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        FillSwapDesc(desc, width, height, DXGI_ALPHA_MODE_PREMULTIPLIED);
        premulCreateHr_ = factory_->CreateSwapChainForHwnd(
            device_->Device(),
            hwnd_,
            &desc,
            nullptr,
            nullptr,
            &swapChain_);
        if (FAILED(premulCreateHr_) || !swapChain_)
        {
            swapChain_.Reset();
            FillSwapDesc(desc, width, height, DXGI_ALPHA_MODE_IGNORE);
            ignoreCreateHr_ = factory_->CreateSwapChainForHwnd(
                device_->Device(),
                hwnd_,
                &desc,
                nullptr,
                nullptr,
                &swapChain_);
            if (FAILED(ignoreCreateHr_) || !swapChain_)
            {
                error = L"CreateSwapChainForHwnd overlay failed premul=" +
                        FormatHresult(premulCreateHr_) + L" ignore=" +
                        FormatHresult(ignoreCreateHr_) +
                        L". Layered DXGI HWND path stopped; do not mix DComp.";
                ReleaseSwapChain();
                alphaMode_ = OverlayPresentAlphaMode::Unspecified;
                return false;
            }
            alphaMode_ = OverlayPresentAlphaMode::Ignore;
            std::wstring dwmError;
            if (!ApplyDwmExtendedFrame(dwmError))
            {
                lastError_ = dwmError;
            }
        }
        else
        {
            ignoreCreateHr_ = S_OK;
            alphaMode_ = OverlayPresentAlphaMode::Premultiplied;
        }

        factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
    }
    else
    {
        HRESULT const hr = swapChain_->ResizeBuffers(
            2,
            width,
            height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0);
        if (FAILED(hr))
        {
            error = L"ResizeBuffers overlay failed " + FormatHresult(hr) + L".";
            ReleaseSwapChain();
            return false;
        }
    }

    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (!BindBackBufferRtv(error))
    {
        ReleaseSwapChain();
        return false;
    }
    return true;
}

bool OverlaySurface::BindBackBufferRtv(std::wstring& error)
{
    rtv_.Reset();
    if (!device_ || !swapChain_)
    {
        error = L"BindBackBufferRtv skipped: swap chain missing.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer)
    {
        error = L"GetBuffer overlay back buffer failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = device_->Device()->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);
    if (FAILED(hr) || !rtv_)
    {
        error = L"CreateRenderTargetView overlay swap chain failed " + FormatHresult(hr) + L".";
        return false;
    }
    return true;
}

bool OverlaySurface::DrawMarker(std::wstring& error)
{
    if (!device_ || !rtv_ || !swapChain_)
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

    unsigned const magentaWidth = (std::max)(180u, surfaceWidth_ / 5u);
    unsigned const magentaHeight = (std::max)(80u, surfaceHeight_ / 10u);
    unsigned const yellowWidth = (std::max)(48u, magentaWidth / 3u);
    unsigned const yellowHeight = (std::max)(160u, surfaceHeight_ / 4u);

    D3D11_RECT magentaRect{
        static_cast<LONG>(kMarkerOriginX),
        static_cast<LONG>(kMarkerOriginY),
        static_cast<LONG>(kMarkerOriginX + magentaWidth),
        static_cast<LONG>(kMarkerOriginY + magentaHeight)};
    if (ClipRect(magentaRect, surfaceWidth_, surfaceHeight_))
    {
        float const magenta[4] = {1.0f, 0.0f, 1.0f, 1.0f};
        context1->ClearView(rtv_.Get(), magenta, &magentaRect, 1);
    }

    D3D11_RECT yellowRect{
        static_cast<LONG>(kMarkerOriginX),
        static_cast<LONG>(kMarkerOriginY + magentaHeight),
        static_cast<LONG>(kMarkerOriginX + yellowWidth),
        static_cast<LONG>(kMarkerOriginY + magentaHeight + yellowHeight)};
    if (ClipRect(yellowRect, surfaceWidth_, surfaceHeight_))
    {
        float const yellow[4] = {1.0f, 1.0f, 0.0f, 1.0f};
        context1->ClearView(rtv_.Get(), yellow, &yellowRect, 1);
    }

    context1->OMSetRenderTargets(0, nullptr, nullptr);
    return Present(error);
}

bool OverlaySurface::Present(std::wstring& error)
{
    if (!swapChain_)
    {
        error = L"DXGI HWND present skipped: swap chain missing.";
        lastPresentResult_ = E_FAIL;
        return false;
    }

    UnbindContextTargets();
    std::uint64_t const presentStart = QueryCounter();
    lastPresentResult_ = swapChain_->Present(1, 0);
    lastPresentUs_ = CounterToMicroseconds(presentStart, QueryCounter());
    if (FAILED(lastPresentResult_))
    {
        error = L"IDXGISwapChain1::Present overlay failed " + FormatHresult(lastPresentResult_) +
                L".";
        return false;
    }
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
