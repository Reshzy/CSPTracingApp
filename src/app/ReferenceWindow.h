#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"
#include "graphics/ImageRenderer.h"

#include <dxgi1_2.h>

#include <cstdint>
#include <string>

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif

namespace tracing::app {

struct ReferenceAffinityStatus
{
    bool setCalled = false;
    int setResult = 0;
    unsigned long setLastError = 0;
    unsigned long readback = 0;
    bool readbackSucceeded = false;
    bool matchesNone = false;
};

inline bool ReferenceAffinityIsNone(ReferenceAffinityStatus const& status) noexcept
{
    return status.setCalled && status.setResult != 0 && status.matchesNone;
}

inline bool OverlayOwnerIsForeground(HWND foreground, HWND control, HWND reference) noexcept
{
    if (foreground == nullptr)
    {
        return false;
    }
    return foreground == control || (reference != nullptr && foreground == reference);
}

// Ordinary movable reference HWND. WDA_NONE only; exclusion stays on the overlay.
// Independent DXGI presentation; shares ImageRenderer's immutable texture.
class ReferenceWindow
{
public:
    ReferenceWindow() = default;
    ~ReferenceWindow();

    ReferenceWindow(ReferenceWindow const&) = delete;
    ReferenceWindow& operator=(ReferenceWindow const&) = delete;
    ReferenceWindow(ReferenceWindow&&) = delete;
    ReferenceWindow& operator=(ReferenceWindow&&) = delete;

    bool Create(HWND owner, tracing::graphics::DeviceResources& device, std::wstring& error);
    void Release();
    void Hide();
    bool Show(tracing::graphics::ImageRenderer& renderer, std::wstring& error);
    bool Present(tracing::graphics::ImageRenderer& renderer, std::wstring& error);

    HWND Handle() const noexcept;
    bool IsVisible() const noexcept;
    ReferenceAffinityStatus const& Affinity() const noexcept;
    tracing::graphics::ImagePlacement Placement() const noexcept;
    unsigned TextureGeneration() const noexcept;
    HRESULT LastHr() const noexcept;
    std::wstring FormatReport() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    bool RegisterReferenceClass(HINSTANCE instance, std::wstring& error);
    bool CreateHwnd(HINSTANCE instance, HWND owner, std::wstring& error);
    bool ApplyNoneAffinity();
    bool EnsureSwapChain(unsigned width, unsigned height, std::wstring& error);
    void UnbindContextTargets() noexcept;
    void ReleaseSwapChain() noexcept;
    bool ClientSize(unsigned& width, unsigned& height) const noexcept;
    void FitToClient(tracing::graphics::ImageRenderer const& renderer) noexcept;

    tracing::graphics::DeviceResources* device_ = nullptr;
    tracing::graphics::ImageRenderer* renderer_ = nullptr;
    HWND owner_ = nullptr;
    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    ReferenceAffinityStatus affinity_{};
    tracing::graphics::ImagePlacement placement_{};
    unsigned bufferWidth_ = 0;
    unsigned bufferHeight_ = 0;
    unsigned textureGeneration_ = 0;
    HRESULT lastHr_ = S_OK;
    bool visible_ = false;
    bool inSize_ = false;
    std::wstring lastError_;
};

inline HWND ReferenceWindow::Handle() const noexcept
{
    return hwnd_;
}

inline bool ReferenceWindow::IsVisible() const noexcept
{
    return visible_;
}

inline ReferenceAffinityStatus const& ReferenceWindow::Affinity() const noexcept
{
    return affinity_;
}

inline tracing::graphics::ImagePlacement ReferenceWindow::Placement() const noexcept
{
    return placement_;
}

inline unsigned ReferenceWindow::TextureGeneration() const noexcept
{
    return textureGeneration_;
}

inline HRESULT ReferenceWindow::LastHr() const noexcept
{
    return lastHr_;
}

} // namespace tracing::app
