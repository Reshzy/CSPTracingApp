#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"

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

enum class OverlayInteractionMode
{
    Tracing,
    Alignment,
};

// Temporary OBS-gate diagnostic only. Default remains exclude-from-capture.
enum class OverlayAffinityMode
{
    ExcludeFromCapture,
    TemporaryNonePositiveControl,
};

inline LRESULT OverlayHitTestCode(OverlayInteractionMode mode) noexcept
{
    return mode == OverlayInteractionMode::Alignment ? HTCLIENT : HTTRANSPARENT;
}

inline bool OverlayUsesTransparentExStyle(OverlayInteractionMode mode) noexcept
{
    return mode == OverlayInteractionMode::Tracing;
}

inline std::wstring FormatOverlayInteractionMode(OverlayInteractionMode mode)
{
    return mode == OverlayInteractionMode::Alignment ? L"alignment" : L"tracing";
}

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
    bool matchesNone = false;
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

inline bool AffinityIsReadyForMode(
    OverlayAffinityStatus const& status,
    OverlayAffinityMode mode) noexcept
{
    if (mode == OverlayAffinityMode::TemporaryNonePositiveControl)
    {
        return status.setCalled && status.setResult != 0 && status.matchesNone;
    }
    return AffinityIsReady(status);
}

inline std::wstring FormatOverlayAffinityMode(OverlayAffinityMode mode)
{
    return mode == OverlayAffinityMode::TemporaryNonePositiveControl
               ? L"temporary-none-positive-control"
               : L"exclude-from-capture";
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

enum class OverlayPresentAlphaMode
{
    Unspecified,
    Premultiplied,
    Ignore,
};

inline std::wstring FormatOverlayPresentAlphaMode(OverlayPresentAlphaMode mode)
{
    switch (mode)
    {
    case OverlayPresentAlphaMode::Premultiplied:
        return L"premultiplied";
    case OverlayPresentAlphaMode::Ignore:
        return L"ignore";
    default:
        return L"unspecified";
    }
}

// Top-level WS_EX_LAYERED overlay presented with CreateSwapChainForHwnd.
// DeviceResources is borrowed. Isolated from DirectComposition and ULW.
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
    void RequestTestPatternAt(OverlayPlacement const& placement);
    void ClearTestPattern() noexcept;
    void SetInteractionMode(OverlayInteractionMode mode);
    bool SetAffinityMode(OverlayAffinityMode mode, std::wstring& error);
    OverlayAffinityMode AffinityMode() const noexcept;
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
    OverlayInteractionMode InteractionMode() const noexcept;
    unsigned ConsumedMouseDown() const noexcept;
    unsigned ConsumedWheel() const noexcept;
    unsigned ConsumedPointerDown() const noexcept;
    bool EnsurePresentSize(unsigned width, unsigned height, std::wstring& error);
    ID3D11RenderTargetView* RenderTargetView() const noexcept;
    unsigned PresentWidth() const noexcept;
    unsigned PresentHeight() const noexcept;
    bool Present(std::wstring& error);
    std::wstring FormatReport() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    bool RegisterOverlayClass(HINSTANCE instance, std::wstring& error);
    bool CreateOverlayWindow(HINSTANCE instance, std::wstring& error);
    bool CreateDxgiFactory(std::wstring& error);
    bool ApplyLayeredRedirection(std::wstring& error);
    bool ApplyDwmExtendedFrame(std::wstring& error);
    bool ApplyExcludeFromCapture();
    bool ApplyNonePositiveControl();
    bool ApplyActiveAffinity();
    bool ApplyDisplayAffinity(unsigned long affinity);
    bool ApplyHitTestStyles();
    bool EnsureSwapChain(unsigned width, unsigned height, std::wstring& error);
    bool BindBackBufferRtv(std::wstring& error);
    void UnbindContextTargets() noexcept;
    void ReleaseSwapChain() noexcept;
    bool DrawMarker(std::wstring& error);
    void HideWindowOnly();
    OverlayPlacement TestPatternPlacement() const;
    LONG_PTR CurrentExStyle() const noexcept;

    DeviceResources* device_ = nullptr;
    HWND controlWindow_ = nullptr;
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    OverlayAffinityStatus affinity_{};
    OverlayAffinityMode affinityMode_ = OverlayAffinityMode::ExcludeFromCapture;
    OverlayVisibilityResult lastVisibility_{};
    OverlayPlacement lastPlacement_{};
    OverlayPresentAlphaMode alphaMode_ = OverlayPresentAlphaMode::Unspecified;
    unsigned surfaceWidth_ = 0;
    unsigned surfaceHeight_ = 0;
    HRESULT lastPresentResult_ = S_OK;
    HRESULT premulCreateHr_ = S_OK;
    HRESULT ignoreCreateHr_ = S_OK;
    unsigned long lastPresentUs_ = 0;
    bool contentReady_ = false;
    bool emergencyHidden_ = false;
    bool testPatternActive_ = false;
    bool visible_ = false;
    bool topmostWhileShown_ = false;
    bool dwmExtendedFrame_ = false;
    OverlayInteractionMode mode_ = OverlayInteractionMode::Tracing;
    unsigned consumedMouseDown_ = 0;
    unsigned consumedWheel_ = 0;
    unsigned consumedPointerDown_ = 0;
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

inline OverlayAffinityMode OverlaySurface::AffinityMode() const noexcept
{
    return affinityMode_;
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

inline OverlayInteractionMode OverlaySurface::InteractionMode() const noexcept
{
    return mode_;
}

inline unsigned OverlaySurface::ConsumedMouseDown() const noexcept
{
    return consumedMouseDown_;
}

inline unsigned OverlaySurface::ConsumedWheel() const noexcept
{
    return consumedWheel_;
}

inline unsigned OverlaySurface::ConsumedPointerDown() const noexcept
{
    return consumedPointerDown_;
}

inline ID3D11RenderTargetView* OverlaySurface::RenderTargetView() const noexcept
{
    return rtv_.Get();
}

inline unsigned OverlaySurface::PresentWidth() const noexcept
{
    return surfaceWidth_;
}

inline unsigned OverlaySurface::PresentHeight() const noexcept
{
    return surfaceHeight_;
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
