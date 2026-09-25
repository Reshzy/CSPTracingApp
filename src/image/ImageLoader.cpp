#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "image/ImageLoader.h"

#include <wrl/client.h>

#include <climits>
#include <cstdio>
#include <new>
#include <utility>

namespace tracing::image {
namespace {

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

void SetFail(
    HRESULT hr,
    ImageLoadReason why,
    std::wstring const& detail,
    ImageLoadReason& reason,
    std::wstring& error)
{
    reason = why;
    error = detail + L" hr=" + FormatHresult(hr);
}

bool ReadExifOrientation(IWICBitmapFrameDecode* frame, unsigned& orientation, bool& present)
{
    orientation = 1;
    present = false;
    if (frame == nullptr)
    {
        return true;
    }

    Microsoft::WRL::ComPtr<IWICMetadataQueryReader> reader;
    HRESULT const readerHr = frame->GetMetadataQueryReader(&reader);
    if (FAILED(readerHr) || !reader)
    {
        return true;
    }

    wchar_t const* const queries[] = {
        L"System.Photo.Orientation",
        L"/app1/ifd/{ushort=274}",
        L"/ifd/{ushort=274}",
    };

    for (wchar_t const* query : queries)
    {
        PROPVARIANT value{};
        HRESULT const hr = reader->GetMetadataByName(query, &value);
        if (SUCCEEDED(hr))
        {
            unsigned parsed = 1;
            if (value.vt == VT_UI2)
            {
                parsed = value.uiVal;
            }
            else if (value.vt == VT_I2)
            {
                parsed = static_cast<unsigned>(value.iVal);
            }
            else if (value.vt == VT_UI4)
            {
                parsed = value.ulVal;
            }
            PropVariantClear(&value);
            if (parsed >= 1 && parsed <= 8)
            {
                orientation = parsed;
                present = true;
                return true;
            }
            return true;
        }
        PropVariantClear(&value);
    }
    return true;
}

} // namespace

DimensionCheck ValidateDecodedDimensions(int width, int height) noexcept
{
    DimensionCheck result{};
    if (width <= 0 || height <= 0)
    {
        result.reason = ImageLoadReason::InvalidDimensions;
        return result;
    }
    if (width > kMaxDecodedAxis || height > kMaxDecodedAxis)
    {
        result.reason = ImageLoadReason::OversizedAxis;
        return result;
    }

    auto const w = static_cast<std::uint64_t>(width);
    auto const h = static_cast<std::uint64_t>(height);
    if (w > (UINT64_MAX / static_cast<std::uint64_t>(kDecodedBytesPerPixel)))
    {
        result.reason = ImageLoadReason::StrideOverflow;
        return result;
    }
    std::uint64_t const stride = w * static_cast<std::uint64_t>(kDecodedBytesPerPixel);
    if (stride > static_cast<std::uint64_t>(INT_MAX))
    {
        result.reason = ImageLoadReason::StrideOverflow;
        return result;
    }
    if (h > 0 && stride > (UINT64_MAX / h))
    {
        result.reason = ImageLoadReason::DecodedByteLimit;
        return result;
    }
    std::uint64_t const bytes = h * stride;
    if (bytes > kMaxDecodedBytes)
    {
        result.reason = ImageLoadReason::DecodedByteLimit;
        return result;
    }

    result.ok = true;
    result.reason = ImageLoadReason::Ok;
    return result;
}

WICBitmapTransformOptions MapExifOrientationToWic(unsigned orientation) noexcept
{
    switch (orientation)
    {
    case 2:
        return WICBitmapTransformFlipHorizontal;
    case 3:
        return WICBitmapTransformRotate180;
    case 4:
        return WICBitmapTransformFlipVertical;
    case 5:
        return static_cast<WICBitmapTransformOptions>(
            WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
    case 6:
        return WICBitmapTransformRotate90;
    case 7:
        return static_cast<WICBitmapTransformOptions>(
            WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
    case 8:
        return WICBitmapTransformRotate270;
    default:
        return WICBitmapTransformRotate0;
    }
}

bool OrientationSwapsAxes(unsigned orientation) noexcept
{
    return orientation == 5 || orientation == 6 || orientation == 7 || orientation == 8;
}

std::wstring FormatImageLoadReason(ImageLoadReason reason)
{
    switch (reason)
    {
    case ImageLoadReason::Ok:
        return L"ok";
    case ImageLoadReason::EmptyPath:
        return L"empty-path";
    case ImageLoadReason::FileOpenFailed:
        return L"file-open-failed";
    case ImageLoadReason::DecoderCreateFailed:
        return L"decoder-create-failed";
    case ImageLoadReason::UnsupportedContainer:
        return L"unsupported-container";
    case ImageLoadReason::FrameDecodeFailed:
        return L"frame-decode-failed";
    case ImageLoadReason::InvalidDimensions:
        return L"invalid-dimensions";
    case ImageLoadReason::OversizedAxis:
        return L"oversized-axis";
    case ImageLoadReason::DecodedByteLimit:
        return L"decoded-byte-limit";
    case ImageLoadReason::StrideOverflow:
        return L"stride-overflow";
    case ImageLoadReason::ConvertFailed:
        return L"convert-failed";
    case ImageLoadReason::CopyFailed:
        return L"copy-failed";
    case ImageLoadReason::OrientationFailed:
        return L"orientation-failed";
    default:
        return L"unknown";
    }
}

std::wstring LeafNameFromPath(std::wstring const& path)
{
    auto const slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return path;
    }
    return path.substr(slash + 1);
}

HRESULT LoadFromPath(
    std::wstring const& path,
    DecodedImage& out,
    ImageLoadReason& reason,
    std::wstring& error)
{
    reason = ImageLoadReason::Ok;
    error.clear();
    if (path.empty())
    {
        SetFail(E_INVALIDARG, ImageLoadReason::EmptyPath, L"LoadFromPath empty path.", reason, error);
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory)
    {
        SetFail(hr, ImageLoadReason::DecoderCreateFailed, L"CoCreateInstance IWICImagingFactory failed.", reason, error);
        return FAILED(hr) ? hr : E_FAIL;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr) || !decoder)
    {
        ImageLoadReason const why = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr || HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) == hr
                                        ? ImageLoadReason::FileOpenFailed
                                        : ImageLoadReason::DecoderCreateFailed;
        SetFail(hr, why, L"CreateDecoderFromFilename failed.", reason, error);
        return FAILED(hr) ? hr : E_FAIL;
    }

    GUID container{};
    hr = decoder->GetContainerFormat(&container);
    if (FAILED(hr))
    {
        SetFail(hr, ImageLoadReason::DecoderCreateFailed, L"GetContainerFormat failed.", reason, error);
        return hr;
    }

    ImageContainer kind = ImageContainer::Unknown;
    if (container == GUID_ContainerFormatPng)
    {
        kind = ImageContainer::Png;
    }
    else if (container == GUID_ContainerFormatJpeg)
    {
        kind = ImageContainer::Jpeg;
    }
    else
    {
        SetFail(
            WINCODEC_ERR_UNKNOWNIMAGEFORMAT,
            ImageLoadReason::UnsupportedContainer,
            L"Only PNG and JPEG are accepted.",
            reason,
            error);
        return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame)
    {
        SetFail(hr, ImageLoadReason::FrameDecodeFailed, L"GetFrame(0) failed.", reason, error);
        return FAILED(hr) ? hr : E_FAIL;
    }

    unsigned orientation = 1;
    bool orientationPresent = false;
    ReadExifOrientation(frame.Get(), orientation, orientationPresent);

    UINT srcW = 0;
    UINT srcH = 0;
    hr = frame->GetSize(&srcW, &srcH);
    if (FAILED(hr))
    {
        SetFail(hr, ImageLoadReason::FrameDecodeFailed, L"GetSize failed.", reason, error);
        return hr;
    }

    UINT expectedW = srcW;
    UINT expectedH = srcH;
    if (OrientationSwapsAxes(orientation))
    {
        expectedW = srcH;
        expectedH = srcW;
    }
    if (expectedW > static_cast<UINT>(INT_MAX) || expectedH > static_cast<UINT>(INT_MAX))
    {
        SetFail(
            HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
            ImageLoadReason::OversizedAxis,
            L"GetSize exceeds int range.",
            reason,
            error);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    DimensionCheck const preCheck =
        ValidateDecodedDimensions(static_cast<int>(expectedW), static_cast<int>(expectedH));
    if (!preCheck.ok)
    {
        SetFail(
            HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE),
            preCheck.reason,
            L"Rejected before CopyPixels: " + FormatImageLoadReason(preCheck.reason) + L" size=" +
                std::to_wstring(expectedW) + L"x" + std::to_wstring(expectedH),
            reason,
            error);
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }

    Microsoft::WRL::ComPtr<IWICBitmapSource> source = frame;
    if (orientationPresent && orientation != 1)
    {
        Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (FAILED(hr) || !rotator)
        {
            SetFail(hr, ImageLoadReason::OrientationFailed, L"CreateBitmapFlipRotator failed.", reason, error);
            return FAILED(hr) ? hr : E_FAIL;
        }
        hr = rotator->Initialize(frame.Get(), MapExifOrientationToWic(orientation));
        if (FAILED(hr))
        {
            SetFail(hr, ImageLoadReason::OrientationFailed, L"IWICBitmapFlipRotator::Initialize failed.", reason, error);
            return hr;
        }
        source = rotator;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter)
    {
        SetFail(hr, ImageLoadReason::ConvertFailed, L"CreateFormatConverter failed.", reason, error);
        return FAILED(hr) ? hr : E_FAIL;
    }

    hr = converter->Initialize(
        source.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        SetFail(hr, ImageLoadReason::ConvertFailed, L"IWICFormatConverter::Initialize to 32bppPBGRA failed.", reason, error);
        return hr;
    }

    WICPixelFormatGUID actualFormat = GUID_WICPixelFormatDontCare;
    hr = converter->GetPixelFormat(&actualFormat);
    if (FAILED(hr) || actualFormat != GUID_WICPixelFormat32bppPBGRA)
    {
        HRESULT const failHr = FAILED(hr) ? hr : WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
        SetFail(failHr, ImageLoadReason::ConvertFailed, L"Output was not 32bppPBGRA.", reason, error);
        return failHr;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr))
    {
        SetFail(hr, ImageLoadReason::ConvertFailed, L"Converter GetSize failed.", reason, error);
        return hr;
    }
    if (width > static_cast<UINT>(INT_MAX) || height > static_cast<UINT>(INT_MAX))
    {
        SetFail(
            HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
            ImageLoadReason::OversizedAxis,
            L"Converter size exceeds int range.",
            reason,
            error);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    DimensionCheck const postCheck =
        ValidateDecodedDimensions(static_cast<int>(width), static_cast<int>(height));
    if (!postCheck.ok)
    {
        SetFail(
            HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE),
            postCheck.reason,
            L"Rejected converter size before CopyPixels: " + FormatImageLoadReason(postCheck.reason),
            reason,
            error);
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }

    int const stride = static_cast<int>(width) * kDecodedBytesPerPixel;
    std::uint64_t const byteCount =
        static_cast<std::uint64_t>(height) * static_cast<std::uint64_t>(stride);
    if (byteCount > static_cast<std::uint64_t>(UINT_MAX))
    {
        SetFail(
            HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
            ImageLoadReason::DecodedByteLimit,
            L"CopyPixels buffer exceeds UINT.",
            reason,
            error);
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    }

    DecodedImage decoded;
    decoded.width = static_cast<int>(width);
    decoded.height = static_cast<int>(height);
    decoded.stride = stride;
    decoded.format = PixelFormatKind::BgraPremultiplied32;
    decoded.orientationNormalized = true;
    decoded.container = kind;
    decoded.sourceLeafName = LeafNameFromPath(path);
    try
    {
        decoded.pixels.resize(static_cast<std::size_t>(byteCount));
    }
    catch (std::bad_alloc const&)
    {
        SetFail(E_OUTOFMEMORY, ImageLoadReason::DecodedByteLimit, L"pixel buffer allocation failed.", reason, error);
        return E_OUTOFMEMORY;
    }

    hr = converter->CopyPixels(
        nullptr,
        static_cast<UINT>(stride),
        static_cast<UINT>(byteCount),
        decoded.pixels.data());
    if (FAILED(hr))
    {
        SetFail(hr, ImageLoadReason::CopyFailed, L"CopyPixels failed.", reason, error);
        return hr;
    }

    out = std::move(decoded);
    reason = ImageLoadReason::Ok;
    error.clear();
    return S_OK;
}

bool LoadedImageSlot::TryLoad(std::wstring const& path, std::wstring& error)
{
    DecodedImage candidate;
    ImageLoadReason reason = ImageLoadReason::Ok;
    HRESULT const hr = LoadFromPath(path, candidate, reason, error);
    lastHr_ = hr;
    lastReason_ = reason;
    lastError_ = error;
    if (FAILED(hr) || reason != ImageLoadReason::Ok)
    {
        return false;
    }
    current_ = std::move(candidate);
    return true;
}

bool LoadedImageSlot::HasImage() const noexcept
{
    return current_.has_value();
}

DecodedImage const* LoadedImageSlot::Image() const noexcept
{
    return current_.has_value() ? &*current_ : nullptr;
}

HRESULT LoadedImageSlot::LastHr() const noexcept
{
    return lastHr_;
}

ImageLoadReason LoadedImageSlot::LastReason() const noexcept
{
    return lastReason_;
}

std::wstring const& LoadedImageSlot::LastError() const noexcept
{
    return lastError_;
}

std::wstring LoadedImageSlot::FormatReport() const
{
    std::wstring text = L"image loaded=";
    text += current_.has_value() ? L"yes" : L"no";
    if (current_.has_value())
    {
        text += L" leaf=\"";
        text += current_->sourceLeafName;
        text += L"\" size=";
        text += std::to_wstring(current_->width);
        text += L"x";
        text += std::to_wstring(current_->height);
        text += L" stride=";
        text += std::to_wstring(current_->stride);
        text += L" format=32bppPBGRA premultiplied orientationNormalized=";
        text += current_->orientationNormalized ? L"yes" : L"no";
        text += L" container=";
        text += current_->container == ImageContainer::Png
                    ? L"png"
                    : (current_->container == ImageContainer::Jpeg ? L"jpeg" : L"unknown");
        text += L" bytes=";
        text += std::to_wstring(current_->pixels.size());
    }
    wchar_t hrBuf[16]{};
    swprintf_s(hrBuf, L"0x%08X", static_cast<unsigned>(lastHr_));
    text += L" lastHr=";
    text += hrBuf;
    text += L" reason=";
    text += FormatImageLoadReason(lastReason_);
    if (!current_.has_value() && !lastError_.empty())
    {
        text += L" error=";
        text += lastError_;
    }
    else if (current_.has_value() && lastReason_ != ImageLoadReason::Ok && !lastError_.empty())
    {
        text += L" lastAttempt=";
        text += lastError_;
        text += L" (previous preserved)";
    }
    text += L" (decode only; not uploaded to overlay)";
    return text;
}

} // namespace tracing::image
