#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"

#include <cstdint>
#include <string>

namespace tracing::graphics {

// Overlay test-marker origin in overlay-client pixels (OverlaySurface constants).
// Used only to sample whether that marker appears in a CSP capture texture.
inline constexpr int kOverlayMarkerClientX = 16;
inline constexpr int kOverlayMarkerClientY = 24;
inline constexpr int kOverlayMarkerWidth = 96;
inline constexpr int kOverlayMarkerHeight = 40;

enum class CaptureSizeMatch
{
    None,
    OuterWindow,
    DwmFrame,
    OuterAndDwm,
};

enum class CapturePreviewLabel
{
    ActualFrame,
    Stale,
    ZeroSize,
    ItemClosed,
    NoFrame,
};

enum class MarkerFeedback
{
    NotSampled,
    Absent,
    Present,
};

struct CaptureToClientMapping
{
    int contentWidth = 0;
    int contentHeight = 0;
    long clientX = 0;
    long clientY = 0;
    long clientWidth = 0;
    long clientHeight = 0;
    long outerX = 0;
    long outerY = 0;
    long outerWidth = 0;
    long outerHeight = 0;
    long dwmX = 0;
    long dwmY = 0;
    long dwmWidth = 0;
    long dwmHeight = 0;
    long clientInOuterX = 0;
    long clientInOuterY = 0;
    long clientInDwmX = 0;
    long clientInDwmY = 0;
    long contentMinusOuterWidth = 0;
    long contentMinusOuterHeight = 0;
    long contentMinusDwmWidth = 0;
    long contentMinusDwmHeight = 0;
    long captureToClientX = 0;
    long captureToClientY = 0;
    CaptureSizeMatch sizeMatch = CaptureSizeMatch::None;
};

// GPU-free: compare WGC content size to outer/DWM bounds. Offsets are measured,
// not hardcoded chrome. Result is capture-to-client mapping, not canvas bounds.
inline CaptureToClientMapping MeasureCaptureToClientMapping(
    int contentWidth,
    int contentHeight,
    long clientX,
    long clientY,
    long clientWidth,
    long clientHeight,
    long outerX,
    long outerY,
    long outerWidth,
    long outerHeight,
    long dwmX,
    long dwmY,
    long dwmWidth,
    long dwmHeight) noexcept
{
    CaptureToClientMapping mapping{};
    mapping.contentWidth = contentWidth;
    mapping.contentHeight = contentHeight;
    mapping.clientX = clientX;
    mapping.clientY = clientY;
    mapping.clientWidth = clientWidth;
    mapping.clientHeight = clientHeight;
    mapping.outerX = outerX;
    mapping.outerY = outerY;
    mapping.outerWidth = outerWidth;
    mapping.outerHeight = outerHeight;
    mapping.dwmX = dwmX;
    mapping.dwmY = dwmY;
    mapping.dwmWidth = dwmWidth;
    mapping.dwmHeight = dwmHeight;
    mapping.clientInOuterX = clientX - outerX;
    mapping.clientInOuterY = clientY - outerY;
    mapping.clientInDwmX = clientX - dwmX;
    mapping.clientInDwmY = clientY - dwmY;
    mapping.contentMinusOuterWidth = static_cast<long>(contentWidth) - outerWidth;
    mapping.contentMinusOuterHeight = static_cast<long>(contentHeight) - outerHeight;
    mapping.contentMinusDwmWidth = static_cast<long>(contentWidth) - dwmWidth;
    mapping.contentMinusDwmHeight = static_cast<long>(contentHeight) - dwmHeight;

    bool const outerMatch = contentWidth > 0 && contentHeight > 0 && contentWidth == outerWidth &&
                            contentHeight == outerHeight;
    bool const dwmMatch = contentWidth > 0 && contentHeight > 0 && contentWidth == dwmWidth &&
                          contentHeight == dwmHeight;
    if (outerMatch && dwmMatch)
    {
        mapping.sizeMatch = CaptureSizeMatch::OuterAndDwm;
        mapping.captureToClientX = mapping.clientInOuterX;
        mapping.captureToClientY = mapping.clientInOuterY;
    }
    else if (outerMatch)
    {
        mapping.sizeMatch = CaptureSizeMatch::OuterWindow;
        mapping.captureToClientX = mapping.clientInOuterX;
        mapping.captureToClientY = mapping.clientInOuterY;
    }
    else if (dwmMatch)
    {
        mapping.sizeMatch = CaptureSizeMatch::DwmFrame;
        mapping.captureToClientX = mapping.clientInDwmX;
        mapping.captureToClientY = mapping.clientInDwmY;
    }
    else
    {
        mapping.sizeMatch = CaptureSizeMatch::None;
        mapping.captureToClientX = mapping.clientInOuterX;
        mapping.captureToClientY = mapping.clientInOuterY;
    }
    return mapping;
}

inline wchar_t const* FormatCaptureSizeMatch(CaptureSizeMatch match) noexcept
{
    switch (match)
    {
    case CaptureSizeMatch::OuterWindow:
        return L"outer-window";
    case CaptureSizeMatch::DwmFrame:
        return L"dwm-frame";
    case CaptureSizeMatch::OuterAndDwm:
        return L"outer-and-dwm";
    case CaptureSizeMatch::None:
        break;
    }
    return L"none";
}

inline std::wstring FormatCaptureToClientMapping(CaptureToClientMapping const& mapping)
{
    return L"captureToClient measured (not canvas) match=" +
           std::wstring(FormatCaptureSizeMatch(mapping.sizeMatch)) + L" content=" +
           std::to_wstring(mapping.contentWidth) + L"x" + std::to_wstring(mapping.contentHeight) +
           L" outer=" + std::to_wstring(mapping.outerWidth) + L"x" +
           std::to_wstring(mapping.outerHeight) + L" dwm=" + std::to_wstring(mapping.dwmWidth) +
           L"x" + std::to_wstring(mapping.dwmHeight) + L" client=" +
           std::to_wstring(mapping.clientWidth) + L"x" + std::to_wstring(mapping.clientHeight) +
           L"\r\n  clientInOuter=(" + std::to_wstring(mapping.clientInOuterX) + L"," +
           std::to_wstring(mapping.clientInOuterY) + L") clientInDwm=(" +
           std::to_wstring(mapping.clientInDwmX) + L"," + std::to_wstring(mapping.clientInDwmY) +
           L") residualOuter=" + std::to_wstring(mapping.contentMinusOuterWidth) + L"x" +
           std::to_wstring(mapping.contentMinusOuterHeight) + L" residualDwm=" +
           std::to_wstring(mapping.contentMinusDwmWidth) + L"x" +
           std::to_wstring(mapping.contentMinusDwmHeight) + L" usingOffset=(" +
           std::to_wstring(mapping.captureToClientX) + L"," +
           std::to_wstring(mapping.captureToClientY) + L")";
}

inline CapturePreviewLabel ClassifyCapturePreviewLabel(
    bool hasOwnedFrame,
    bool stale,
    int contentWidth,
    int contentHeight,
    bool itemClosed) noexcept
{
    if (itemClosed)
    {
        return CapturePreviewLabel::ItemClosed;
    }
    if (contentWidth <= 0 || contentHeight <= 0)
    {
        return CapturePreviewLabel::ZeroSize;
    }
    if (stale)
    {
        return CapturePreviewLabel::Stale;
    }
    if (!hasOwnedFrame)
    {
        return CapturePreviewLabel::NoFrame;
    }
    return CapturePreviewLabel::ActualFrame;
}

inline wchar_t const* FormatCapturePreviewLabel(CapturePreviewLabel label) noexcept
{
    switch (label)
    {
    case CapturePreviewLabel::ActualFrame:
        return L"ACTUAL";
    case CapturePreviewLabel::Stale:
        return L"STALE";
    case CapturePreviewLabel::ZeroSize:
        return L"ZERO SIZE";
    case CapturePreviewLabel::ItemClosed:
        return L"ITEM CLOSED";
    case CapturePreviewLabel::NoFrame:
        return L"NO FRAME";
    }
    return L"UNKNOWN";
}

inline wchar_t const* FormatMarkerFeedback(MarkerFeedback feedback) noexcept
{
    switch (feedback)
    {
    case MarkerFeedback::Absent:
        return L"no";
    case MarkerFeedback::Present:
        return L"yes";
    case MarkerFeedback::NotSampled:
        break;
    }
    return L"not-sampled";
}

// Ordinary top-level debug window (WDA_NONE). Off by default. Not the tracing overlay.
class CapturePreview
{
public:
    CapturePreview() = default;
    ~CapturePreview();

    CapturePreview(CapturePreview const&) = delete;
    CapturePreview& operator=(CapturePreview const&) = delete;
    CapturePreview(CapturePreview&&) = delete;
    CapturePreview& operator=(CapturePreview&&) = delete;

    bool Create(HWND owner, DeviceResources& device, std::wstring& error);
    void Release();
    void SetEnabled(bool enabled);
    void Hide();
    bool Present(
        ID3D11Texture2D* ownedTexture,
        CapturePreviewLabel label,
        std::uint64_t sequence,
        std::int64_t captureTicks,
        CaptureToClientMapping const& mapping,
        std::wstring& error);

    HWND Handle() const noexcept;
    bool IsEnabled() const noexcept;
    bool IsVisible() const noexcept;
    MarkerFeedback LastMarkerFeedback() const noexcept;
    std::wstring FormatReport() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    bool RegisterPreviewClass(HINSTANCE instance, std::wstring& error);
    bool CreatePreviewWindow(HINSTANCE instance, HWND owner, std::wstring& error);
    bool EnsureCpuSurfaces(unsigned width, unsigned height, DXGI_FORMAT format, std::wstring& error);
    void ReleaseCpuSurfaces() noexcept;
    void ReleaseDib() noexcept;
    bool CopyTextureToDib(ID3D11Texture2D* ownedTexture, unsigned width, unsigned height, std::wstring& error);
    void ScanMarkerFeedback(CaptureToClientMapping const& mapping);
    void UpdateTitle();
    void Paint(HDC hdc, RECT const& client) const;

    DeviceResources* device_ = nullptr;
    HWND owner_ = nullptr;
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_;
    HDC dibDc_ = nullptr;
    HBITMAP dibBitmap_ = nullptr;
    HGDIOBJ dibOld_ = nullptr;
    void* dibBits_ = nullptr;
    unsigned bitmapWidth_ = 0;
    unsigned bitmapHeight_ = 0;
    DXGI_FORMAT stagingFormat_ = DXGI_FORMAT_UNKNOWN;
    CapturePreviewLabel label_ = CapturePreviewLabel::NoFrame;
    std::uint64_t sequence_ = 0;
    std::int64_t captureTicks_ = 0;
    MarkerFeedback markerFeedback_ = MarkerFeedback::NotSampled;
    bool enabled_ = false;
    bool visible_ = false;
    bool hasBitmap_ = false;
};

inline HWND CapturePreview::Handle() const noexcept
{
    return hwnd_;
}

inline bool CapturePreview::IsEnabled() const noexcept
{
    return enabled_;
}

inline bool CapturePreview::IsVisible() const noexcept
{
    return visible_;
}

inline MarkerFeedback CapturePreview::LastMarkerFeedback() const noexcept
{
    return markerFeedback_;
}

} // namespace tracing::graphics
