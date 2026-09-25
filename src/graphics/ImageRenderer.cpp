#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/ImageRenderer.h"

#include <d3d11.h>

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace tracing::graphics {
namespace {

constexpr unsigned kMaxDimension = 16384;
constexpr unsigned kBytesPerPixel = 4;

struct PlacementConstants
{
    float overlayWidth = 0.0f;
    float overlayHeight = 0.0f;
    float translationX = 0.0f;
    float translationY = 0.0f;
    float imageWidth = 0.0f;
    float imageHeight = 0.0f;
    float scale = 1.0f;
    float opacity = 1.0f;
};

static_assert(sizeof(PlacementConstants) == 32, "PlacementConstants must match Image.hlsl cbuffer");

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

std::wstring ModuleDirectory()
{
    DWORD capacity = 260;
    std::wstring path(capacity, L'\0');
    for (;;)
    {
        DWORD const copied = GetModuleFileNameW(nullptr, path.data(), capacity);
        if (copied == 0)
        {
            return {};
        }
        if (copied < capacity - 1)
        {
            path.resize(copied);
            break;
        }
        capacity *= 2;
        path.assign(capacity, L'\0');
    }

    size_t const slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return {};
    }
    return path.substr(0, slash);
}

} // namespace

ImageRenderer::~ImageRenderer()
{
    Release();
}

bool ImageRenderer::Create(DeviceResources& device, std::wstring& error)
{
    error.clear();
    Release();

    if (!device.IsReady() || device.Device() == nullptr || device.ImmediateContext() == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ImageRenderer needs a ready BGRA D3D11 device.";
        lastError_ = error;
        return false;
    }

    device_ = &device;
    std::wstring const directory = ModuleDirectory();
    if (directory.empty())
    {
        lastHr_ = HRESULT_FROM_WIN32(GetLastError());
        error = L"GetModuleFileNameW failed; cannot locate shader objects.";
        lastError_ = error;
        device_ = nullptr;
        return false;
    }
    vsPath_ = directory + L"\\ImageVS.cso";
    psPath_ = directory + L"\\ImagePS.cso";
    if (!CreatePipeline(error))
    {
        lastError_ = error;
        Release();
        return false;
    }

    lastHr_ = S_OK;
    lastError_.clear();
    return true;
}

void ImageRenderer::Release()
{
    ReleaseTexture();
    constants_.Reset();
    rasterizer_.Reset();
    blend_.Reset();
    sampler_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    device_ = nullptr;
}

bool ImageRenderer::Upload(tracing::image::DecodedImage const& image, std::wstring& error)
{
    error.clear();
    if (!PipelineReady() || device_->Device() == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ImageRenderer::Upload skipped: pipeline not ready.";
        lastError_ = error;
        return false;
    }
    if (image.format != tracing::image::PixelFormatKind::BgraPremultiplied32)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::Upload requires 32bppPBGRA premultiplied pixels.";
        lastError_ = error;
        return false;
    }
    if (image.width <= 0 || image.height <= 0 ||
        image.width > static_cast<int>(kMaxDimension) ||
        image.height > static_cast<int>(kMaxDimension))
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::Upload rejected invalid texture dimensions.";
        lastError_ = error;
        return false;
    }

    int const expectedStride = image.width * static_cast<int>(kBytesPerPixel);
    if (image.stride != expectedStride)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::Upload requires tight stride width*4.";
        lastError_ = error;
        return false;
    }

    size_t const expectedBytes =
        static_cast<size_t>(image.height) * static_cast<size_t>(image.stride);
    if (image.pixels.size() != expectedBytes)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::Upload pixel buffer size does not match stride*height.";
        lastError_ = error;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(image.width);
    desc.Height = static_cast<UINT>(image.height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = image.pixels.data();
    init.SysMemPitch = static_cast<UINT>(image.stride);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device_->Device()->CreateTexture2D(&desc, &init, &texture);
    if (FAILED(hr) || !texture)
    {
        lastHr_ = hr;
        error = L"CreateTexture2D immutable reference failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    hr = device_->Device()->CreateShaderResourceView(texture.Get(), nullptr, &srv);
    if (FAILED(hr) || !srv)
    {
        lastHr_ = hr;
        error = L"CreateShaderResourceView failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        return false;
    }

    texture_ = std::move(texture);
    srv_ = std::move(srv);
    textureWidth_ = image.width;
    textureHeight_ = image.height;
    ++textureGeneration_;
    lastHr_ = S_OK;
    lastError_.clear();
    return true;
}

bool ImageRenderer::DrawAndPresent(
    OverlaySurface& overlay,
    OverlayPlacement const& overlayPlacement,
    OverlayInteractionMode mode,
    std::wstring& error)
{
    error.clear();
    if (overlay.Handle() == nullptr)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::DrawAndPresent needs the overlay HWND.";
        lastError_ = error;
        return false;
    }
    if (!PipelineReady() || !HasTexture() || device_->ImmediateContext() == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ImageRenderer::DrawAndPresent skipped: pipeline or texture missing.";
        lastError_ = error;
        return false;
    }
    if (overlayPlacement.width <= 0 || overlayPlacement.height <= 0)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::DrawAndPresent skipped: overlay size is empty.";
        lastError_ = error;
        return false;
    }

    unsigned const width = static_cast<unsigned>(overlayPlacement.width);
    unsigned const height = static_cast<unsigned>(overlayPlacement.height);
    if (!overlay.EnsurePresentSize(width, height, error))
    {
        lastHr_ = E_FAIL;
        lastError_ = error;
        return false;
    }
    if (overlay.RenderTargetView() == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ImageRenderer::DrawAndPresent skipped: overlay RTV missing.";
        lastError_ = error;
        return false;
    }

    float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (mode == OverlayInteractionMode::Alignment)
    {
        clear[3] = 32.0f / 255.0f;
    }
    if (!DrawToRtv(overlay.RenderTargetView(), width, height, placement_, opacity_, clear, error))
    {
        lastError_ = error;
        return false;
    }

    if (!overlay.Present(error))
    {
        lastHr_ = E_FAIL;
        lastError_ = error;
        return false;
    }

    lastHr_ = S_OK;
    lastError_.clear();
    return true;
}

bool ImageRenderer::DrawQuad(
    ID3D11RenderTargetView* rtv,
    unsigned width,
    unsigned height,
    ImagePlacement const& placement,
    float opacity,
    std::wstring& error)
{
    error.clear();
    float const opaqueGray[4] = {32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f, 1.0f};
    return DrawToRtv(rtv, width, height, placement, ClampOpacity(opacity), opaqueGray, error);
}

bool ImageRenderer::DrawToRtv(
    ID3D11RenderTargetView* rtv,
    unsigned width,
    unsigned height,
    ImagePlacement const& placement,
    float opacity,
    float const clearColor[4],
    std::wstring& error)
{
    if (rtv == nullptr)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::DrawToRtv needs a render-target view.";
        lastError_ = error;
        return false;
    }
    if (!PipelineReady() || !HasTexture() || device_->ImmediateContext() == nullptr)
    {
        lastHr_ = E_FAIL;
        error = L"ImageRenderer::DrawToRtv skipped: pipeline or texture missing.";
        lastError_ = error;
        return false;
    }
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension)
    {
        lastHr_ = E_INVALIDARG;
        error = L"ImageRenderer::DrawToRtv skipped: target size is invalid.";
        lastError_ = error;
        return false;
    }

    ID3D11DeviceContext* const context = device_->ImmediateContext();
    ID3D11RenderTargetView* views[] = {rtv};
    context->OMSetRenderTargets(1, views, nullptr);
    context->ClearRenderTargetView(rtv, clearColor);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(rasterizer_.Get());

    float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(blend_.Get(), blendFactor, 0xffffffff);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context->Map(constants_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr) || mapped.pData == nullptr)
    {
        lastHr_ = hr;
        error = L"Map placement constants failed " + FormatHresult(hr) + L".";
        lastError_ = error;
        context->OMSetRenderTargets(0, nullptr, nullptr);
        return false;
    }

    ImagePlacement used = placement;
    if (used.scale <= 0.0)
    {
        used.scale = 1.0;
    }

    PlacementConstants constants{};
    constants.overlayWidth = static_cast<float>(width);
    constants.overlayHeight = static_cast<float>(height);
    constants.translationX = static_cast<float>(used.offsetX);
    constants.translationY = static_cast<float>(used.offsetY);
    constants.imageWidth = static_cast<float>(textureWidth_);
    constants.imageHeight = static_cast<float>(textureHeight_);
    constants.scale = static_cast<float>(used.scale);
    constants.opacity = ClampOpacity(opacity);
    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(constants_.Get(), 0);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = {constants_.Get()};
    context->VSSetConstantBuffers(0, 1, cbs);
    context->PSSetConstantBuffers(0, 1, cbs);
    ID3D11ShaderResourceView* srvs[] = {srv_.Get()};
    context->PSSetShaderResources(0, 1, srvs);
    ID3D11SamplerState* samplers[] = {sampler_.Get()};
    context->PSSetSamplers(0, 1, samplers);
    context->Draw(4, 0);

    ID3D11ShaderResourceView* noneSrv[] = {nullptr};
    context->PSSetShaderResources(0, 1, noneSrv);
    context->OMSetRenderTargets(0, nullptr, nullptr);

    lastHr_ = S_OK;
    lastError_.clear();
    return true;
}

std::wstring ImageRenderer::FormatReport() const
{
    std::wstring text = L"imageRenderer pipeline=";
    text += PipelineReady() ? L"yes" : L"no";
    text += L" uploaded=";
    text += HasTexture() ? L"yes" : L"no";
    text += L" generation=";
    text += std::to_wstring(textureGeneration_);
    text += L" size=";
    text += std::to_wstring(textureWidth_);
    text += L"x";
    text += std::to_wstring(textureHeight_);
    text += L"\r\nformat=32bppPBGRA premultiplied once blend=ONE/INV_SRC_ALPHA clear=transparent";
    text += L"\r\nopacity=";
    wchar_t opacityBuffer[32]{};
    swprintf_s(opacityBuffer, L"%.3f", static_cast<double>(opacity_));
    text += opacityBuffer;
    text += L" placement offset=(";
    wchar_t placeBuffer[64]{};
    swprintf_s(placeBuffer, L"%.2f,%.2f) scale=%.5f", placement_.offsetX, placement_.offsetY, placement_.scale);
    text += placeBuffer;
    text += L" (explicit; texture unchanged by fit/reset/opacity)";
    text += L"\r\nlastHr=";
    text += FormatHresult(lastHr_);
    text += L" present=overlay-dxgi-hwnd";
    text += L" vs=";
    text += vsPath_.empty() ? L"(none)" : vsPath_;
    if (!lastError_.empty())
    {
        text += L"\r\nimageRenderer error: ";
        text += lastError_;
    }
    return text;
}

bool ImageRenderer::CreatePipeline(std::wstring& error)
{
    std::vector<std::uint8_t> vsBytes;
    std::vector<std::uint8_t> psBytes;
    if (!LoadShaderFile(vsPath_, vsBytes, error) || !LoadShaderFile(psPath_, psBytes, error))
    {
        return false;
    }

    HRESULT hr = device_->Device()->CreateVertexShader(
        vsBytes.data(), vsBytes.size(), nullptr, &vertexShader_);
    if (FAILED(hr) || !vertexShader_)
    {
        lastHr_ = hr;
        error = L"CreateVertexShader ImageVS.cso failed " + FormatHresult(hr) + L".";
        return false;
    }

    hr = device_->Device()->CreatePixelShader(psBytes.data(), psBytes.size(), nullptr, &pixelShader_);
    if (FAILED(hr) || !pixelShader_)
    {
        lastHr_ = hr;
        error = L"CreatePixelShader ImagePS.cso failed " + FormatHresult(hr) + L".";
        return false;
    }

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->Device()->CreateSamplerState(&sampler, &sampler_);
    if (FAILED(hr) || !sampler_)
    {
        lastHr_ = hr;
        error = L"CreateSamplerState failed " + FormatHresult(hr) + L".";
        return false;
    }

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = device_->Device()->CreateBlendState(&blend, &blend_);
    if (FAILED(hr) || !blend_)
    {
        lastHr_ = hr;
        error = L"CreateBlendState premul failed " + FormatHresult(hr) + L".";
        return false;
    }

    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    hr = device_->Device()->CreateRasterizerState(&raster, &rasterizer_);
    if (FAILED(hr) || !rasterizer_)
    {
        lastHr_ = hr;
        error = L"CreateRasterizerState failed " + FormatHresult(hr) + L".";
        return false;
    }

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = sizeof(PlacementConstants);
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device_->Device()->CreateBuffer(&cb, nullptr, &constants_);
    if (FAILED(hr) || !constants_)
    {
        lastHr_ = hr;
        error = L"CreateBuffer placement constants failed " + FormatHresult(hr) + L".";
        return false;
    }

    lastHr_ = S_OK;
    return true;
}

bool ImageRenderer::LoadShaderFile(
    std::wstring const& path,
    std::vector<std::uint8_t>& bytes,
    std::wstring& error)
{
    bytes.clear();
    HANDLE const file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        unsigned long const code = GetLastError();
        lastHr_ = HRESULT_FROM_WIN32(code);
        error = L"Failed to open shader object " + path + L" (Win32 " + std::to_wstring(code) + L").";
        return false;
    }

    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024)
    {
        unsigned long const code = GetLastError();
        CloseHandle(file);
        lastHr_ = HRESULT_FROM_WIN32(code);
        error = L"Invalid shader object size " + path + L".";
        return false;
    }

    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    BOOL const ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (ok == FALSE || read != bytes.size())
    {
        unsigned long const code = GetLastError();
        lastHr_ = HRESULT_FROM_WIN32(code);
        error = L"ReadFile shader object failed " + path + L" (Win32 " + std::to_wstring(code) + L").";
        bytes.clear();
        return false;
    }
    return true;
}

void ImageRenderer::ReleaseTexture() noexcept
{
    srv_.Reset();
    texture_.Reset();
    textureWidth_ = 0;
    textureHeight_ = 0;
}

} // namespace tracing::graphics
