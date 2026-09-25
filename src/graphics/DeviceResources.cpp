#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"

#include <dxgi.h>

#include <cstdio>

namespace tracing::graphics {
namespace {

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

std::wstring FormatHex32(std::uint32_t value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%04X", value);
    return buffer;
}

} // namespace

DeviceResources::~DeviceResources()
{
    Release();
}

HRESULT DeviceResources::TryCreateDevice(UINT flags)
{
    constexpr D3D_FEATURE_LEVEL kLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL actual = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    HRESULT const hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        kLevels,
        ARRAYSIZE(kLevels),
        D3D11_SDK_VERSION,
        &device,
        &actual,
        &context);
    if (SUCCEEDED(hr))
    {
        device_.Attach(device);
        context_.Attach(context);
        info_.featureLevel = static_cast<int>(actual);
    }
    return hr;
}

void DeviceResources::CaptureAdapterIdentity()
{
    info_.adapter = {};
    if (!device_)
    {
        return;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device_.As(&dxgiDevice)))
    {
        return;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter)) || !adapter)
    {
        return;
    }

    DXGI_ADAPTER_DESC desc{};
    if (FAILED(adapter->GetDesc(&desc)))
    {
        return;
    }

    info_.adapter.description = desc.Description;
    info_.adapter.vendorId = desc.VendorId;
    info_.adapter.deviceId = desc.DeviceId;
    info_.adapter.subsystemId = desc.SubSysId;
    info_.adapter.revision = desc.Revision;
    info_.adapter.adapterLuid =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(desc.AdapterLuid.HighPart)) << 32) |
        desc.AdapterLuid.LowPart;
}

bool DeviceResources::Create(std::wstring& error)
{
    error.clear();
    Release();
    policy_ = DeviceLifecyclePolicy(policy_.MaxRetries());
    info_ = {};

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    info_.debugLayerRequested = true;
#endif

    HRESULT hr = TryCreateDevice(flags);
    info_.lastCreateResult = hr;
    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0)
    {
        policy_.Apply(DeviceLifecycleEvent::DebugLayerMissing);
        info_.debugLayerInstalled = false;
        info_.debugLayerEnabled = false;
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = TryCreateDevice(flags);
        info_.lastCreateResult = hr;
    }

    if (FAILED(hr))
    {
        policy_.Apply(DeviceLifecycleEvent::CreateFailed);
        error = L"D3D11CreateDevice failed HRESULT=" + FormatHresult(hr) +
                L" (hardware BGRA device; debug-layer absence is not this failure).";
        return false;
    }

    debug_.Reset();
    if (SUCCEEDED(device_.As(&debug_)) && debug_)
    {
        info_.debugLayerEnabled = true;
        info_.debugLayerInstalled = true;
    }
    else if (info_.debugLayerRequested && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0)
    {
        info_.debugLayerEnabled = true;
        info_.debugLayerInstalled = true;
    }

    CaptureAdapterIdentity();
    info_.lastDeviceRemovedReason = device_->GetDeviceRemovedReason();
    policy_.Apply(DeviceLifecycleEvent::CreateSucceeded);
    return true;
}

void DeviceResources::Release()
{
    bool const alreadyReleased =
        policy_.State() == DeviceLifecycleState::Released && !device_ && !context_ && !debug_;
    if (alreadyReleased)
    {
        return;
    }

    if (device_ && !debug_)
    {
        (void)device_.As(&debug_);
    }

    if (context_)
    {
        context_->ClearState();
        context_->Flush();
        context_.Reset();
    }

    device_.Reset();

    if (debug_)
    {
        debug_->ReportLiveDeviceObjects(
            static_cast<D3D11_RLDO_FLAGS>(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL));
        info_.liveObjectsReported = true;
        debug_.Reset();
    }

    policy_.Apply(DeviceLifecycleEvent::Shutdown);
}

bool DeviceResources::IsReady() const noexcept
{
    return policy_.State() == DeviceLifecycleState::Ready && device_ && context_;
}

bool DeviceResources::CheckDeviceRemoved(HRESULT& reason) noexcept
{
    reason = S_OK;
    if (!device_)
    {
        return false;
    }

    reason = device_->GetDeviceRemovedReason();
    info_.lastDeviceRemovedReason = reason;
    if (reason == S_OK)
    {
        return false;
    }

    policy_.Apply(DeviceLifecycleEvent::DeviceRemoved);
    return true;
}

ID3D11Device* DeviceResources::Device() const noexcept
{
    return device_.Get();
}

ID3D11DeviceContext* DeviceResources::ImmediateContext() const noexcept
{
    return context_.Get();
}

DeviceInfo const& DeviceResources::Info() const noexcept
{
    return info_;
}

DeviceLifecycleState DeviceResources::LifecycleState() const noexcept
{
    return policy_.State();
}

DeviceLifecyclePolicy const& DeviceResources::Policy() const noexcept
{
    return policy_;
}

std::wstring DeviceResources::FormatReport() const
{
    std::wstring adapter = info_.adapter.description.empty() ? L"(none)" : info_.adapter.description;
    std::wstring debug;
    if (info_.debugLayerEnabled)
    {
        debug = L"enabled";
    }
    else if (info_.debugLayerRequested && !info_.debugLayerInstalled)
    {
        debug = L"absent (not a failure)";
    }
    else if (info_.debugLayerRequested)
    {
        debug = L"requested";
    }
    else
    {
        debug = L"not requested";
    }

    return L"D3D11 adapter=\"" + adapter + L"\" vendor=0x" + FormatHex32(info_.adapter.vendorId) +
           L" device=0x" + FormatHex32(info_.adapter.deviceId) + L" featureLevel=" +
           FormatFeatureLevel(info_.featureLevel) + L" state=" +
           FormatDeviceLifecycleState(policy_.State()) + L"\r\n" + L"debugLayer requested=" +
           (info_.debugLayerRequested ? L"yes" : L"no") + L" " + debug + L" create=" +
           FormatHresult(info_.lastCreateResult) + L" removed=" +
           FormatHresult(info_.lastDeviceRemovedReason) + L" liveObjectsReported=" +
           (info_.liveObjectsReported ? L"yes" : L"no");
}

} // namespace tracing::graphics
