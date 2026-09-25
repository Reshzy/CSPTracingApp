#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <wincodec.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tracing::image {

// Decoded CPU buffer contract (independent of overlay / D3D):
// - GUID_WICPixelFormat32bppPBGRA: 8-bit BGRA, premultiplied once
// - top-left origin after EXIF/WIC orientation normalization
// - stride = width * 4 (checked); pixels.size() = height * stride
// - ImageLoader / LoadedImageSlot own the vector; callers borrow

inline constexpr int kMaxDecodedAxis = 16384;
inline constexpr std::uint64_t kMaxDecodedBytes = 256ull * 1024ull * 1024ull;
inline constexpr int kDecodedBytesPerPixel = 4;

enum class ImageLoadReason
{
    Ok,
    EmptyPath,
    FileOpenFailed,
    DecoderCreateFailed,
    UnsupportedContainer,
    FrameDecodeFailed,
    InvalidDimensions,
    OversizedAxis,
    DecodedByteLimit,
    StrideOverflow,
    ConvertFailed,
    CopyFailed,
    OrientationFailed,
};

enum class ImageContainer
{
    Unknown,
    Png,
    Jpeg,
};

enum class PixelFormatKind
{
    BgraPremultiplied32,
};

struct DecodedImage
{
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<std::uint8_t> pixels;
    PixelFormatKind format = PixelFormatKind::BgraPremultiplied32;
    bool orientationNormalized = true;
    ImageContainer container = ImageContainer::Unknown;
    std::wstring sourceLeafName;
};

struct DimensionCheck
{
    bool ok = false;
    ImageLoadReason reason = ImageLoadReason::InvalidDimensions;
};

DimensionCheck ValidateDecodedDimensions(int width, int height) noexcept;

// Maps EXIF orientation 1-8 to WICBitmapTransformOptions. Unknown/0/1 -> Rotate0.
WICBitmapTransformOptions MapExifOrientationToWic(unsigned orientation) noexcept;
bool OrientationSwapsAxes(unsigned orientation) noexcept;

std::wstring FormatImageLoadReason(ImageLoadReason reason);
std::wstring LeafNameFromPath(std::wstring const& path);

HRESULT LoadFromPath(
    std::wstring const& path,
    DecodedImage& out,
    ImageLoadReason& reason,
    std::wstring& error);

class LoadedImageSlot
{
public:
    bool TryLoad(std::wstring const& path, std::wstring& error);
    bool HasImage() const noexcept;
    DecodedImage const* Image() const noexcept;
    HRESULT LastHr() const noexcept;
    ImageLoadReason LastReason() const noexcept;
    std::wstring const& LastError() const noexcept;
    std::wstring FormatReport() const;

private:
    std::optional<DecodedImage> current_;
    HRESULT lastHr_ = S_OK;
    ImageLoadReason lastReason_ = ImageLoadReason::Ok;
    std::wstring lastError_;
};

} // namespace tracing::image
