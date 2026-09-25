#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"

#include <dcomp.h>
#include <dxgi1_2.h>

#include <cstdint>
#include <string>

namespace tracing::graphics {

enum class OverlayVisibilityDecision
{
    Show,
    Hide,
};

enum class OverlayVisibilityReason
{
    ShownOnTarget,
    ShownTestPattern,
    EmergencyHidden,
    AffinityFailed,
    ContentNotReady,
    TargetUnusable,
    UnrelatedForeground,
};

enum class OverlayObsVerification
{
    NotRun,
};

struct OverlayVisibilityInput
{
    bool targetUsable = false;
    bool targetForeground = false;
    bool ownerControlForeground = false;
    bool affinityOk = false;
    bool emergencyHidden = false;
    bool contentReady = false;
    bool testPatternWithoutTarget = false;
};

struct OverlayVisibilityResult
{
    OverlayVisibilityDecision decision = OverlayVisibilityDecision::Hide;
    OverlayVisibilityReason reason = OverlayVisibilityReason::ContentNotReady;
};

struct OverlayAffinityStatus
{
    bool setCalled = false;
    int setResult = 0;
    unsigned long setLastError = 0;
    unsigned long readback = 0;
    bool readbackSucceeded = false;
    bool matchesExcludeFromCapture = false;
};

struct OverlayPlacement
{
    long x = 0;
    long y = 0;
    long width = 0;
    long height = 0;
};

inline bool AffinityIsReady(OverlayAffinityStatus const& status) noexcept
{
    return status.setCalled && status.setResult != 0 && status.matchesExcludeFromCapture;
}

// GPU-free policy. OBS verification is not an input and cannot authorize a show.
inline OverlayVisibilityResult DecideOverlayVisibility(OverlayVisibilityInput const& input) noexcept
{
    OverlayVisibilityResult result{};
    if (input.emergencyHidden)
    {
        result.decision = OverlayVisibilityDecision::Hide;
        result.reason = OverlayVisibilityReason::EmergencyHidden;
        return result;
    }
    if (!input.affinityOk)
    {
        result.decision = OverlayVisibilityDecision::Hide;
        result.reason = OverlayVisibilityReason::AffinityFailed;
        return result;
    }
    if (!input.contentReady)
    {
        result.decision = OverlayVisibilityDecision::Hide;
        result.reason = OverlayVisibilityReason::ContentNotReady;
        return result;
    }
    if (input.targetUsable && (input.targetForeground || input.ownerControlForeground))
    {
        result.decision = OverlayVisibilityDecision::Show;
        result.reason = OverlayVisibilityReason::ShownOnTarget;
        return result;
    }
    if (input.testPatternWithoutTarget && !input.targetUsable)
    {
        result.decision = OverlayVisibilityDecision::Show;
        result.reason = OverlayVisibilityReason::ShownTestPattern;
        return result;
    }
    if (!input.targetUsable)
    {
        result.decision = OverlayVisibilityDecision::Hide;
        result.reason = OverlayVisibilityReason::TargetUnusable;
        return result;
    }

    result.decision = OverlayVisibilityDecision::Hide;
    result.reason = OverlayVisibilityReason::UnrelatedForeground;
    return result;
}

inline std::wstring FormatOverlayVisibilityDecision(OverlayVisibilityDecision decision)
{
    return decision == OverlayVisibilityDecision::Show ? L"Show" : L"Hide";
}

inline std::wstring FormatOverlayVisibilityReason(OverlayVisibilityReason reason)
{
    switch (reason)
    {
    case OverlayVisibilityReason::ShownOnTarget:
        return L"shown-on-target";
    case OverlayVisibilityReason::ShownTestPattern:
        return L"shown-test-pattern";
    case OverlayVisibilityReason::EmergencyHidden:
        return L"emergency-hidden";
    case OverlayVisibilityReason::AffinityFailed:
        return L"affinity-failed";
    case OverlayVisibilityReason::ContentNotReady:
        return L"content-not-ready";
    case OverlayVisibilityReason::TargetUnusable:
        return L"target-unusable";
    case OverlayVisibilityReason::UnrelatedForeground:
        return L"unrelated-foreground";
    }
    return L"unknown";
}

inline std::wstring FormatObsVerification(OverlayObsVerification)
{
    return L"NOT RUN";
}

// Top-level DirectComposition overlay. DeviceResources is borrowed.
class OverlaySurface
{
public:
    OverlaySurface() = default;
    ~OverlaySurface();

    OverlaySurface(OverlaySurface const&) = delete;
    OverlaySurface& operator=(OverlaySurface const&) = delete;
    OverlaySurface(OverlaySurface&&) = delete;
    OverlaySurface& operator=(OverlaySurface&&) = delete;

    bool Create(HWND controlWindow, DeviceResources& device, std::wstring& error);
    void Release();
    void EmergencyHide();
    void ClearEmergencyHide() noexcept;
    void RequestTestPattern();
    void ClearTestPattern() noexcept;
    void UpdatePlacementAndVisibility(
        OverlayPlacement const& placement,
        bool targetUsable,
        bool targetForeground,
        bool ownerControlForeground,
        bool hasTarget,
        std::wstring& error);

    HWND Handle() const noexcept;
    bool IsVisible() const noexcept;
    bool EmergencyHidden() const noexcept;
    bool TestPatternActive() const noexcept;
    bool ContentReady() const noexcept;
    OverlayAffinityStatus const& Affinity() const noexcept;
    OverlayObsVerification ObsVerification() const noexcept;
    OverlayVisibilityResult LastVisibility() const noexcept;
    OverlayPlacement const& LastPlacement() const noexcept;
    std::wstring FormatReport() const;

private:
    bool RegisterOverlayClass(HINSTANCE instance, std::wstring& error);
    bool CreateOverlayWindow(HINSTANCE instance, std::wstring& error);
    bool CreateComposition(std::wstring& error);
    bool ApplyExcludeFromCapture();
    bool EnsureSwapChainSize(unsigned width, unsigned height, std::wstring& error);
    bool BindBackBuffer(std::wstring& error);
    bool DrawMarker(std::wstring& error);
    void HideWindowOnly();
    OverlayPlacement TestPatternPlacement() const;

    DeviceResources* device_ = nullptr;
    HWND controlWindow_ = nullptr;
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> compositionDevice_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> compositionTarget_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> compositionVisual_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    OverlayAffinityStatus affinity_{};
    OverlayVisibilityResult lastVisibility_{};
    OverlayPlacement lastPlacement_{};
    unsigned swapWidth_ = 0;
    unsigned swapHeight_ = 0;
    HRESULT lastPresentResult_ = S_OK;
    bool contentReady_ = false;
    bool emergencyHidden_ = false;
    bool testPatternActive_ = false;
    bool visible_ = false;
    std::wstring lastError_;
};

inline HWND OverlaySurface::Handle() const noexcept
{
    return hwnd_;
}

inline bool OverlaySurface::IsVisible() const noexcept
{
    return visible_;
}

inline bool OverlaySurface::EmergencyHidden() const noexcept
{
    return emergencyHidden_;
}

inline bool OverlaySurface::TestPatternActive() const noexcept
{
    return testPatternActive_;
}

inline bool OverlaySurface::ContentReady() const noexcept
{
    return contentReady_;
}

inline OverlayAffinityStatus const& OverlaySurface::Affinity() const noexcept
{
    return affinity_;
}

inline OverlayObsVerification OverlaySurface::ObsVerification() const noexcept
{
    return OverlayObsVerification::NotRun;
}

inline OverlayVisibilityResult OverlaySurface::LastVisibility() const noexcept
{
    return lastVisibility_;
}

inline OverlayPlacement const& OverlaySurface::LastPlacement() const noexcept
{
    return lastPlacement_;
}

inline void OverlaySurface::ClearEmergencyHide() noexcept
{
    emergencyHidden_ = false;
}

inline void OverlaySurface::ClearTestPattern() noexcept
{
    testPatternActive_ = false;
}

} // namespace tracing::graphics
