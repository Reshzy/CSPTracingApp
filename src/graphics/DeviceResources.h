#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

namespace tracing::graphics {

enum class DeviceLifecycleState
{
    Uninitialized,
    Ready,
    Removed,
    Retrying,
    Failed,
    Released,
};

enum class DeviceLifecycleEvent
{
    CreateSucceeded,
    CreateFailed,
    DebugLayerMissing,
    DeviceRemoved,
    Retry,
    Shutdown,
};

// GPU-free lifecycle policy. Debug-layer absence is informational and never Failed.
class DeviceLifecyclePolicy
{
public:
    explicit DeviceLifecyclePolicy(unsigned maxRetries = 2) noexcept;

    DeviceLifecycleState Apply(DeviceLifecycleEvent event) noexcept;
    DeviceLifecycleState State() const noexcept;
    unsigned RetryCount() const noexcept;
    unsigned MaxRetries() const noexcept;
    bool DebugLayerMissing() const noexcept;
    bool CanRetry() const noexcept;

private:
    DeviceLifecycleState state_ = DeviceLifecycleState::Uninitialized;
    unsigned maxRetries_ = 2;
    unsigned retryCount_ = 0;
    bool debugLayerMissing_ = false;
};

std::wstring FormatDeviceLifecycleState(DeviceLifecycleState state);
std::wstring FormatFeatureLevel(int featureLevel);

struct AdapterIdentity
{
    std::wstring description;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t subsystemId = 0;
    std::uint32_t revision = 0;
    std::uint64_t adapterLuid = 0;
};

struct DeviceInfo
{
    AdapterIdentity adapter;
    int featureLevel = 0;
    bool debugLayerRequested = false;
    bool debugLayerEnabled = false;
    bool debugLayerInstalled = false;
    bool liveObjectsReported = false;
    HRESULT lastCreateResult = S_OK;
    HRESULT lastDeviceRemovedReason = S_OK;
};

// Exclusive owner of the D3D11 device and its immediate context. Accessors are
// borrowed and must be used only on the graphics owner thread.
class DeviceResources
{
public:
    DeviceResources() = default;
    ~DeviceResources();

    DeviceResources(DeviceResources const&) = delete;
    DeviceResources& operator=(DeviceResources const&) = delete;
    DeviceResources(DeviceResources&&) = delete;
    DeviceResources& operator=(DeviceResources&&) = delete;

    bool Create(std::wstring& error);
    void Release();
    bool IsReady() const noexcept;
    bool CheckDeviceRemoved(HRESULT& reason) noexcept;

    ID3D11Device* Device() const noexcept;
    ID3D11DeviceContext* ImmediateContext() const noexcept;

    DeviceInfo const& Info() const noexcept;
    DeviceLifecycleState LifecycleState() const noexcept;
    DeviceLifecyclePolicy const& Policy() const noexcept;
    std::wstring FormatReport() const;

private:
    HRESULT TryCreateDevice(UINT flags);
    void CaptureAdapterIdentity();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Debug> debug_;
    DeviceLifecyclePolicy policy_{};
    DeviceInfo info_{};
};

inline DeviceLifecyclePolicy::DeviceLifecyclePolicy(unsigned maxRetries) noexcept
    : maxRetries_(maxRetries)
{
}

inline DeviceLifecycleState DeviceLifecyclePolicy::State() const noexcept
{
    return state_;
}

inline unsigned DeviceLifecyclePolicy::RetryCount() const noexcept
{
    return retryCount_;
}

inline unsigned DeviceLifecyclePolicy::MaxRetries() const noexcept
{
    return maxRetries_;
}

inline bool DeviceLifecyclePolicy::DebugLayerMissing() const noexcept
{
    return debugLayerMissing_;
}

inline bool DeviceLifecyclePolicy::CanRetry() const noexcept
{
    return state_ == DeviceLifecycleState::Removed && retryCount_ < maxRetries_;
}

inline DeviceLifecycleState DeviceLifecyclePolicy::Apply(DeviceLifecycleEvent event) noexcept
{
    if (event == DeviceLifecycleEvent::Shutdown)
    {
        state_ = DeviceLifecycleState::Released;
        return state_;
    }

    if (event == DeviceLifecycleEvent::DebugLayerMissing)
    {
        debugLayerMissing_ = true;
        return state_;
    }

    switch (state_)
    {
    case DeviceLifecycleState::Uninitialized:
    case DeviceLifecycleState::Retrying:
        if (event == DeviceLifecycleEvent::CreateSucceeded)
        {
            state_ = DeviceLifecycleState::Ready;
        }
        else if (event == DeviceLifecycleEvent::CreateFailed)
        {
            state_ = DeviceLifecycleState::Failed;
        }
        else if (event == DeviceLifecycleEvent::DeviceRemoved)
        {
            state_ = DeviceLifecycleState::Removed;
        }
        break;

    case DeviceLifecycleState::Ready:
        if (event == DeviceLifecycleEvent::DeviceRemoved)
        {
            state_ = DeviceLifecycleState::Removed;
        }
        else if (event == DeviceLifecycleEvent::CreateFailed)
        {
            state_ = DeviceLifecycleState::Failed;
        }
        break;

    case DeviceLifecycleState::Removed:
        if (event == DeviceLifecycleEvent::Retry)
        {
            if (retryCount_ < maxRetries_)
            {
                ++retryCount_;
                state_ = DeviceLifecycleState::Retrying;
            }
            else
            {
                state_ = DeviceLifecycleState::Failed;
            }
        }
        break;

    case DeviceLifecycleState::Failed:
    case DeviceLifecycleState::Released:
        break;
    }

    return state_;
}

inline std::wstring FormatDeviceLifecycleState(DeviceLifecycleState state)
{
    switch (state)
    {
    case DeviceLifecycleState::Uninitialized:
        return L"Uninitialized";
    case DeviceLifecycleState::Ready:
        return L"Ready";
    case DeviceLifecycleState::Removed:
        return L"Removed";
    case DeviceLifecycleState::Retrying:
        return L"Retrying";
    case DeviceLifecycleState::Failed:
        return L"Failed";
    case DeviceLifecycleState::Released:
        return L"Released";
    }
    return L"Unknown";
}

inline std::wstring FormatFeatureLevel(int featureLevel)
{
    if (featureLevel == D3D_FEATURE_LEVEL_11_1)
    {
        return L"11_1";
    }
    if (featureLevel == D3D_FEATURE_LEVEL_11_0)
    {
        return L"11_0";
    }
    return L"unknown";
}

} // namespace tracing::graphics
