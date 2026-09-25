#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"
#include "graphics/OverlaySurface.h"
#include "image/ImageLoader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tracing::graphics {

// GPU-free explicit placement in overlay-local pixels. Not M_RD / tracking.
struct ImagePlacement
{
    double offsetX = 0.0;
    double offsetY = 0.0;
    double scale = 1.0;
};

inline float ClampOpacity(float opacity) noexcept
{
    if (opacity < 0.0f)
    {
        return 0.0f;
    }
    if (opacity > 1.0f)
    {
        return 1.0f;
    }
    return opacity;
}

inline ImagePlacement ResetPlacement() noexcept
{
    return ImagePlacement{};
}

inline ImagePlacement FitPlacement(
    double imageWidth,
    double imageHeight,
    double overlayWidth,
    double overlayHeight) noexcept
{
    ImagePlacement placement{};
    if (imageWidth <= 0.0 || imageHeight <= 0.0 || overlayWidth <= 0.0 || overlayHeight <= 0.0)
    {
        return placement;
    }

    double const scaleX = overlayWidth / imageWidth;
    double const scaleY = overlayHeight / imageHeight;
    placement.scale = scaleX < scaleY ? scaleX : scaleY;
    placement.offsetX = (overlayWidth - imageWidth * placement.scale) * 0.5;
    placement.offsetY = (overlayHeight - imageHeight * placement.scale) * 0.5;
    return placement;
}

// Uploads a 32bppPBGRA CPU buffer once to an immutable texture and draws a
// textured quad onto the existing layered overlay HWND. Placement/opacity update
// constants only. Borrows DeviceResources on the graphics owner thread.
class ImageRenderer
{
public:
    ImageRenderer() = default;
    ~ImageRenderer();

    ImageRenderer(ImageRenderer const&) = delete;
    ImageRenderer& operator=(ImageRenderer const&) = delete;
    ImageRenderer(ImageRenderer&&) = delete;
    ImageRenderer& operator=(ImageRenderer&&) = delete;

    bool Create(DeviceResources& device, std::wstring& error);
    void Release();
    bool Upload(tracing::image::DecodedImage const& image, std::wstring& error);
    bool DrawAndPresent(
        HWND overlay,
        OverlayPlacement const& overlayPlacement,
        OverlayInteractionMode mode,
        std::wstring& error);
    bool DrawQuad(
        ID3D11RenderTargetView* rtv,
        unsigned width,
        unsigned height,
        ImagePlacement const& placement,
        float opacity,
        std::wstring& error);

    ID3D11ShaderResourceView* ShaderResourceView() const noexcept;
    bool PipelineReady() const noexcept;
    bool HasTexture() const noexcept;
    unsigned TextureGeneration() const noexcept;
    int TextureWidth() const noexcept;
    int TextureHeight() const noexcept;
    float Opacity() const noexcept;
    ImagePlacement Placement() const noexcept;
    HRESULT LastHr() const noexcept;
    void SetOpacity(float opacity) noexcept;
    void SetPlacement(ImagePlacement const& placement) noexcept;
    std::wstring FormatReport() const;

private:
    bool CreatePipeline(std::wstring& error);
    bool LoadShaderFile(std::wstring const& path, std::vector<std::uint8_t>& bytes, std::wstring& error);
    bool DrawToRtv(
        ID3D11RenderTargetView* rtv,
        unsigned width,
        unsigned height,
        ImagePlacement const& placement,
        float opacity,
        float const clearColor[4],
        std::wstring& error);
    bool EnsurePresentSize(unsigned width, unsigned height, std::wstring& error);
    bool PresentLayered(HWND overlay, OverlayPlacement const& overlayPlacement, std::wstring& error);
    void ReleasePresentSurfaces() noexcept;
    void ReleaseDib() noexcept;
    void ReleaseTexture() noexcept;

    DeviceResources* device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> presentTexture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    HDC dibDc_ = nullptr;
    HBITMAP dibBitmap_ = nullptr;
    HGDIOBJ dibOld_ = nullptr;
    void* dibBits_ = nullptr;
    unsigned presentWidth_ = 0;
    unsigned presentHeight_ = 0;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    unsigned textureGeneration_ = 0;
    float opacity_ = 1.0f;
    ImagePlacement placement_{};
    HRESULT lastHr_ = S_OK;
    unsigned long lastReadbackUs_ = 0;
    unsigned long lastUlwUs_ = 0;
    std::wstring lastError_;
    std::wstring vsPath_;
    std::wstring psPath_;
};

inline ID3D11ShaderResourceView* ImageRenderer::ShaderResourceView() const noexcept
{
    return srv_.Get();
}

inline bool ImageRenderer::PipelineReady() const noexcept
{
    return device_ != nullptr && vertexShader_ && pixelShader_ && sampler_ && blend_ && rasterizer_ &&
           constants_;
}

inline bool ImageRenderer::HasTexture() const noexcept
{
    return texture_ && srv_ && textureWidth_ > 0 && textureHeight_ > 0;
}

inline unsigned ImageRenderer::TextureGeneration() const noexcept
{
    return textureGeneration_;
}

inline int ImageRenderer::TextureWidth() const noexcept
{
    return textureWidth_;
}

inline int ImageRenderer::TextureHeight() const noexcept
{
    return textureHeight_;
}

inline float ImageRenderer::Opacity() const noexcept
{
    return opacity_;
}

inline ImagePlacement ImageRenderer::Placement() const noexcept
{
    return placement_;
}

inline HRESULT ImageRenderer::LastHr() const noexcept
{
    return lastHr_;
}

inline void ImageRenderer::SetOpacity(float opacity) noexcept
{
    opacity_ = ClampOpacity(opacity);
}

inline void ImageRenderer::SetPlacement(ImagePlacement const& placement) noexcept
{
    placement_ = placement;
    if (placement_.scale <= 0.0)
    {
        placement_.scale = 1.0;
    }
}

} // namespace tracing::graphics
