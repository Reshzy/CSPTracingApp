#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "app/ReferenceWindow.h"

#include <cstdio>

namespace tracing::app {
namespace {

constexpr wchar_t kReferenceClass[] = L"TracingAppReferenceWindow";
constexpr unsigned kMaxDimension = 16384;
constexpr unsigned kDefaultWidth = 640;
constexpr unsigned kDefaultHeight = 480;

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

} // namespace

ReferenceWindow::~ReferenceWindow()
{
    Release();
}

bool ReferenceWindow::RegisterReferenceClass(HINSTANCE instance, std::wstring& error)
{
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, kReferenceClass, &existing) != FALSE)
    {
        return true;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kReferenceClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        error = L"RegisterClassExW ReferenceWindow failed (Win32 " + std::to_wstring(GetLastError()) +
                L").";
        return false;
    }
    return true;
}

bool ReferenceWindow::CreateHwnd(HINSTANCE instance, HWND owner, std::wstring& error)
{
    hwnd_ = CreateWindowExW(
        0,
        kReferenceClass,
        L"TracingApp Reference",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        static_cast<int>(kDefaultWidth),
        static_cast<int>(kDefaultHeight),
        owner,
        nullptr,
        instance,
        this);
    if (hwnd_ == nullptr)
    {
        error = L"CreateWindowExW ReferenceWindow failed (Win32 " + std::to_wstring(GetLastError()) +
                L").";
        return false;
    }

    if (!ApplyNoneAffinity())
    {
        error = lastError_;
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
    return true;
}

bool ReferenceWindow::ApplyNoneAffinity()
{
    affinity_ = {};
    affinity_.setCalled = true;
    SetLastError(0);
    BOOL const setResult = SetWindowDisplayAffinity(hwnd_, WDA_NONE);
    affinity_.setLastError = GetLastError();
    affinity_.setResult = setResult != FALSE ? 1 : 0;

    DWORD readback = 0;
    BOOL const got = GetWindowDisplayAffinity(hwnd_, &readback);
    affinity_.readbackSucceeded = got != FALSE;
    affinity_.readback = readback;
    affinity_.matchesNone = affinity_.readbackSucceeded && readback == WDA_NONE;

    if (!ReferenceAffinityIsNone(affinity_))
    {
        lastError_ = L"SetWindowDisplayAffinity(WDA_NONE) failed or readback mismatch (Win32 " +
                     std::to_wstring(affinity_.setLastError) + L").";
        lastHr_ = HRESULT_FROM_WIN32(affinity_.setLastError);
        return false;
    }
    return true;
}

bool ReferenceWindow::Create(
    HWND owner,
    tracing::graphics::DeviceResources& device,
    std::wstring& error)
{
    error.clear();
    Release();
    if (!device.IsReady() || device.Device() == nullptr)
    {
        error = L"ReferenceWindow requires a ready D3D11 device.";
        lastHr_ = E_FAIL;
        lastError_ = error;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device.Device()->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr) || !dxgiDevice)
    {
        lastHr_ = hr;
        error = L"QueryInterface IDXGIDevice failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr) || !adapter)
    {
        lastHr_ = hr;
        error = L"IDXGIDevice::GetAdapter failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        return false;
    }

    hr = adapter->GetParent(IID_PPV_ARGS(&factory_));
    if (FAILED(hr) || !factory_)
    {
        lastHr_ = hr;
        error = L"IDXGIAdapter::GetParent IDXGIFactory2 failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        return false;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterReferenceClass(instance, error))
    {
        lastError_ = error;
        factory_.Reset();
        return false;
    }
    if (!CreateHwnd(instance, owner, error))
    {
        lastError_ = error;
        factory_.Reset();
        return false;
    }

    device_ = &device;
    owner_ = owner;
    lastHr_ = S_OK;
    lastError_.clear();
    return true;
}

void ReferenceWindow::UnbindContextTargets() noexcept
{
    if (device_ == nullptr || device_->ImmediateContext() == nullptr)
    {
        return;
    }
    ID3D11RenderTargetView* none[] = {nullptr};
    device_->ImmediateContext()->OMSetRenderTargets(0, none, nullptr);
}

void ReferenceWindow::ReleaseSwapChain() noexcept
{
    UnbindContextTargets();
    rtv_.Reset();
    swapChain_.Reset();
    bufferWidth_ = 0;
    bufferHeight_ = 0;
}

void ReferenceWindow::Release()
{
    if (hwnd_ != nullptr)
    {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    ReleaseSwapChain();
    factory_.Reset();
    device_ = nullptr;
    renderer_ = nullptr;
    owner_ = nullptr;
    visible_ = false;
    textureGeneration_ = 0;
    placement_ = {};
    affinity_ = {};
    lastError_.clear();
}

void ReferenceWindow::Hide()
{
    if (hwnd_ != nullptr)
    {
        ShowWindow(hwnd_, SW_HIDE);
    }
    visible_ = false;
}

bool ReferenceWindow::ClientSize(unsigned& width, unsigned& height) const noexcept
{
    width = 0;
    height = 0;
    if (hwnd_ == nullptr)
    {
        return false;
    }
    RECT client{};
    if (GetClientRect(hwnd_, &client) == FALSE)
    {
        return false;
    }
    long const w = client.right - client.left;
    long const h = client.bottom - client.top;
    if (w <= 0 || h <= 0)
    {
        return false;
    }
    width = static_cast<unsigned>(w);
    height = static_cast<unsigned>(h);
    return true;
}

void ReferenceWindow::FitToClient(tracing::graphics::ImageRenderer const& renderer) noexcept
{
    unsigned width = 0;
    unsigned height = 0;
    if (!ClientSize(width, height))
    {
        placement_ = tracing::graphics::ResetPlacement();
        return;
    }
    placement_ = tracing::graphics::FitPlacement(
        static_cast<double>(renderer.TextureWidth()),
        static_cast<double>(renderer.TextureHeight()),
        static_cast<double>(width),
        static_cast<double>(height));
}

bool ReferenceWindow::EnsureSwapChain(unsigned width, unsigned height, std::wstring& error)
{
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ReferenceWindow swap-chain size is invalid.";
        lastError_ = error;
        return false;
    }
    if (swapChain_ && rtv_ && width == bufferWidth_ && height == bufferHeight_)
    {
        return true;
    }
    if (!device_ || device_->Device() == nullptr || !factory_)
    {
        lastHr_ = E_FAIL;
        error = L"ReferenceWindow swap chain needs a ready D3D11 device.";
        lastError_ = error;
        return false;
    }

    UnbindContextTargets();
    rtv_.Reset();

    if (!swapChain_)
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        HRESULT hr = factory_->CreateSwapChainForHwnd(
            device_->Device(),
            hwnd_,
            &desc,
            nullptr,
            nullptr,
            &swapChain_);
        if (FAILED(hr) || !swapChain_)
        {
            lastHr_ = hr;
            error = L"CreateSwapChainForHwnd reference failed " + FormatHresult(hr) + L".";
            lastError_ = error;
            return false;
        }

        factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
    }
    else
    {
        HRESULT hr = swapChain_->ResizeBuffers(
            2,
            width,
            height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0);
        if (FAILED(hr))
        {
            lastHr_ = hr;
            error = L"ResizeBuffers reference failed " + FormatHresult(hr) + L".";
            lastError_ = error;
            ReleaseSwapChain();
            return false;
        }
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer)
    {
        lastHr_ = hr;
        error = L"GetBuffer reference back buffer failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        ReleaseSwapChain();
        return false;
    }

    hr = device_->Device()->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);
    if (FAILED(hr) || !rtv_)
    {
        lastHr_ = hr;
        error = L"CreateRenderTargetView reference failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        ReleaseSwapChain();
        return false;
    }

    bufferWidth_ = width;
    bufferHeight_ = height;
    lastHr_ = S_OK;
    return true;
}

bool ReferenceWindow::Present(tracing::graphics::ImageRenderer& renderer, std::wstring& error)
{
    error.clear();
    renderer_ = &renderer;
    if (!visible_ || hwnd_ == nullptr)
    {
        return true;
    }
    if (inSize_)
    {
        return true;
    }
    if (!renderer.HasTexture() || renderer.ShaderResourceView() == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ReferenceWindow::Present skipped: no shared reference texture.";
        lastError_ = error;
        return false;
    }

    unsigned width = 0;
    unsigned height = 0;
    if (!ClientSize(width, height))
    {
        return true;
    }

    inSize_ = true;
    bool const ok = EnsureSwapChain(width, height, error);
    if (!ok)
    {
        inSize_ = false;
        return false;
    }

    FitToClient(renderer);
    textureGeneration_ = renderer.TextureGeneration();
    if (!renderer.DrawQuad(rtv_.Get(), width, height, placement_, 1.0f, error))
    {
        lastHr_ = renderer.LastHr();
        lastError_ = error;
        inSize_ = false;
        return false;
    }

    HRESULT hr = swapChain_->Present(1, 0);
    lastHr_ = hr;
    inSize_ = false;
    if (FAILED(hr))
    {
        error = L"IDXGISwapChain1::Present reference failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        return false;
    }
    lastError_.clear();
    return true;
}

bool ReferenceWindow::Show(tracing::graphics::ImageRenderer& renderer, std::wstring& error)
{
    error.clear();
    if (hwnd_ == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ReferenceWindow::Show skipped: HWND missing.";
        lastError_ = error;
        return false;
    }
    if (!renderer.HasTexture())
    {
        lastHr_ = E_FAIL;
        error = L"ReferenceWindow::Show skipped: no uploaded image.";
        lastError_ = error;
        return false;
    }

    renderer_ = &renderer;
    visible_ = true;
    ShowWindow(hwnd_, SW_SHOW);
    return Present(renderer, error);
}

std::wstring ReferenceWindow::FormatReport() const
{
    std::wstring text = L"reference hwnd=";
    wchar_t hwndBuffer[32]{};
    swprintf_s(hwndBuffer, L"0x%p", static_cast<void*>(hwnd_));
    text += hwndBuffer;
    text += L" visible=";
    text += visible_ ? L"yes" : L"no";
    text += L" affinity=WDA_NONE set=";
    text += affinity_.setCalled ? (affinity_.setResult != 0 ? L"ok" : L"fail") : L"no";
    text += L" win32=";
    text += std::to_wstring(affinity_.setLastError);
    text += L" readback=";
    wchar_t readbackBuffer[16]{};
    swprintf_s(readbackBuffer, L"0x%08X", affinity_.readback);
    text += readbackBuffer;
    text += L" matchNone=";
    text += affinity_.matchesNone ? L"yes" : L"no";
    text += L" (exclusion belongs only to overlay HWND)";
    text += L"\r\n  client=";
    text += std::to_wstring(bufferWidth_);
    text += L"x";
    text += std::to_wstring(bufferHeight_);
    text += L" independentFit offset=(";
    wchar_t placeBuffer[64]{};
    swprintf_s(
        placeBuffer,
        L"%.2f,%.2f) scale=%.5f",
        placement_.offsetX,
        placement_.offsetY,
        placement_.scale);
    text += placeBuffer;
    text += L" opacity=1.000 textureGeneration=";
    text += std::to_wstring(textureGeneration_);
    text += L" lastHr=";
    text += FormatHresult(lastHr_);
    if (!lastError_.empty())
    {
        text += L"\r\n  reference error: ";
        text += lastError_;
    }
    return text;
}

LRESULT CALLBACK ReferenceWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ReferenceWindow* self = nullptr;
    if (message == WM_NCCREATE)
    {
        auto const* create = reinterpret_cast<CREATESTRUCTW const*>(lParam);
        self = static_cast<ReferenceWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr)
        {
            self->hwnd_ = hwnd;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    self = reinterpret_cast<ReferenceWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            return 0;
        }
        if (self->visible_ && self->renderer_ != nullptr)
        {
            std::wstring error;
            self->Present(*self->renderer_, error);
        }
        return 0;
    case WM_CLOSE:
        self->Hide();
        return 0;
    case WM_DESTROY:
        self->hwnd_ = nullptr;
        self->visible_ = false;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace tracing::app
