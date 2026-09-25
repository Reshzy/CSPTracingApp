#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/OverlaySurface.h"

#include <d3d11_1.h>

#include <cstdio>

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
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

#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif

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

    if (!CreateComposition(error))
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
    rtv_.Reset();
    compositionVisual_.Reset();
    compositionTarget_.Reset();
    compositionDevice_.Reset();
    swapChain_.Reset();
    swapWidth_ = 0;
    swapHeight_ = 0;
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
    input.contentReady = contentReady_ && hwnd_ != nullptr && swapChain_;
    input.testPatternWithoutTarget = testPatternActive_ && !hasTarget;
    lastVisibility_ = DecideOverlayVisibility(input);

    if (lastVisibility_.decision != OverlayVisibilityDecision::Show)
    {
        HideWindowOnly();
        return;
    }

    unsigned width = static_cast<unsigned>(lastPlacement_.width);
    unsigned height = static_cast<unsigned>(lastPlacement_.height);
    if (!EnsureSwapChainSize(width, height, error) || !DrawMarker(error))
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
    text += L" present=";
    text += FormatHresult(lastPresentResult_);
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
    DWORD const exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP |
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
        return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
        if (surface != nullptr)
        {
            ++surface->consumedMouseDown_;
        }
        return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        if (surface != nullptr)
        {
            ++surface->consumedWheel_;
        }
        return 0;
    case WM_POINTERDOWN:
        if (surface != nullptr)
        {
            ++surface->consumedPointerDown_;
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

bool OverlaySurface::CreateComposition(std::wstring& error)
{
    ID3D11Device* const device = device_->Device();
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr) || !dxgiDevice)
    {
        error = L"QueryInterface IDXGIDevice failed " + FormatHresult(hr) + L".";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr) || !adapter)
    {
        error = L"IDXGIDevice::GetAdapter failed " + FormatHresult(hr) + L".";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory)
    {
        error = L"IDXGIAdapter::GetParent IDXGIFactory2 failed " + FormatHresult(hr) + L".";
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = kDefaultWidth;
    desc.Height = kDefaultHeight;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = factory->CreateSwapChainForComposition(device, &desc, nullptr, &swapChain_);
    if (FAILED(hr) || !swapChain_)
    {
        error = L"CreateSwapChainForComposition failed " + FormatHresult(hr) + L".";
        return false;
    }
    swapWidth_ = kDefaultWidth;
    swapHeight_ = kDefaultHeight;

    hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&compositionDevice_));
    if (FAILED(hr) || !compositionDevice_)
    {
        error = L"DCompositionCreateDevice failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = compositionDevice_->CreateTargetForHwnd(hwnd_, TRUE, &compositionTarget_);
    if (FAILED(hr) || !compositionTarget_)
    {
        error = L"CreateTargetForHwnd failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = compositionDevice_->CreateVisual(&compositionVisual_);
    if (FAILED(hr) || !compositionVisual_)
    {
        error = L"CreateVisual failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = compositionVisual_->SetContent(swapChain_.Get());
    if (FAILED(hr))
    {
        error = L"IDCompositionVisual::SetContent failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = compositionTarget_->SetRoot(compositionVisual_.Get());
    if (FAILED(hr))
    {
        error = L"IDCompositionTarget::SetRoot failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = compositionDevice_->Commit();
    if (FAILED(hr))
    {
        error = L"IDCompositionDevice::Commit failed " + FormatHresult(hr) + L".";
        return false;
    }

    return BindBackBuffer(error);
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

bool OverlaySurface::EnsureSwapChainSize(unsigned width, unsigned height, std::wstring& error)
{
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension)
    {
        error = L"Overlay size is invalid.";
        return false;
    }
    if (swapChain_ && width == swapWidth_ && height == swapHeight_ && rtv_)
    {
        return true;
    }

    rtv_.Reset();
    HRESULT const hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        error = L"IDXGISwapChain::ResizeBuffers failed " + FormatHresult(hr) + L".";
        swapWidth_ = 0;
        swapHeight_ = 0;
        return false;
    }
    swapWidth_ = width;
    swapHeight_ = height;
    return BindBackBuffer(error);
}

bool OverlaySurface::BindBackBuffer(std::wstring& error)
{
    rtv_.Reset();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer)
    {
        error = L"IDXGISwapChain::GetBuffer failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = device_->Device()->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);
    if (FAILED(hr) || !rtv_)
    {
        error = L"CreateRenderTargetView failed " + FormatHresult(hr) + L".";
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
    float const clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context1->ClearRenderTargetView(rtv_.Get(), clear);

    if (mode_ == OverlayInteractionMode::Alignment)
    {
        D3D11_RECT strip{0, 0, static_cast<LONG>(swapWidth_), 8};
        if (ClipRect(strip, swapWidth_, swapHeight_))
        {
            float const cyan[4] = {0.0f, 1.0f, 1.0f, 1.0f};
            context1->ClearView(rtv_.Get(), cyan, &strip, 1);
        }
    }

    D3D11_RECT magentaRect{
        static_cast<LONG>(kMagentaX),
        static_cast<LONG>(kMagentaY),
        static_cast<LONG>(kMagentaX + kMagentaWidth),
        static_cast<LONG>(kMagentaY + kMagentaHeight)};
    if (ClipRect(magentaRect, swapWidth_, swapHeight_))
    {
        float const magenta[4] = {1.0f, 0.0f, 1.0f, 1.0f};
        context1->ClearView(rtv_.Get(), magenta, &magentaRect, 1);
    }

    D3D11_RECT yellowRect{
        static_cast<LONG>(kYellowX),
        static_cast<LONG>(kYellowY),
        static_cast<LONG>(kYellowX + kYellowWidth),
        static_cast<LONG>(kYellowY + kYellowHeight)};
    if (ClipRect(yellowRect, swapWidth_, swapHeight_))
    {
        float const yellow[4] = {1.0f, 1.0f, 0.0f, 1.0f};
        context1->ClearView(rtv_.Get(), yellow, &yellowRect, 1);
    }

    context1->OMSetRenderTargets(0, nullptr, nullptr);

    lastPresentResult_ = swapChain_->Present(1, 0);
    if (FAILED(lastPresentResult_))
    {
        error = L"IDXGISwapChain::Present failed " + FormatHresult(lastPresentResult_) + L".";
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
