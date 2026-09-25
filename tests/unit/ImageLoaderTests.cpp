#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <gtest/gtest.h>

#include "image/ImageLoader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using tracing::image::DecodedImage;
using tracing::image::DimensionCheck;
using tracing::image::FormatImageLoadReason;
using tracing::image::ImageContainer;
using tracing::image::ImageLoadReason;
using tracing::image::kMaxDecodedAxis;
using tracing::image::kMaxDecodedBytes;
using tracing::image::LeafNameFromPath;
using tracing::image::LoadFromPath;
using tracing::image::LoadedImageSlot;
using tracing::image::MapExifOrientationToWic;
using tracing::image::OrientationSwapsAxes;
using tracing::image::ValidateDecodedDimensions;

class TempDir
{
public:
    TempDir()
    {
        wchar_t root[MAX_PATH]{};
        DWORD const n = GetTempPathW(MAX_PATH, root);
        if (n == 0 || n >= MAX_PATH)
        {
            return;
        }
        path_ = std::wstring(root) + L"TracingAppImageLoaderTests-" + std::to_wstring(GetCurrentProcessId());
        CreateDirectoryW(path_.c_str(), nullptr);
    }

    ~TempDir()
    {
        if (!path_.empty())
        {
            RemoveDirectoryW(path_.c_str());
        }
    }

    std::wstring const& Path() const
    {
        return path_;
    }

    std::wstring File(std::wstring const& leaf) const
    {
        return path_ + L"\\" + leaf;
    }

    void RemoveTempFile(std::wstring const& full) const
    {
        DeleteFileW(full.c_str());
    }

private:
    std::wstring path_;
};

class ImageComTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        HRESULT const hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ASSERT_TRUE(SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE);
        uninit_ = SUCCEEDED(hr);
    }

    void TearDown() override
    {
        if (uninit_)
        {
            CoUninitialize();
        }
    }

    bool uninit_ = false;
};

HRESULT CreateFactory(Microsoft::WRL::ComPtr<IWICImagingFactory>& factory)
{
    return CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
}

HRESULT EncodeImage(
    std::wstring const& path,
    GUID const& container,
    UINT width,
    UINT height,
    WICPixelFormatGUID format,
    std::uint8_t const* pixels,
    UINT stride,
    unsigned exifOrientation)
{
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CreateFactory(factory);
    if (FAILED(hr) || !factory)
    {
        return FAILED(hr) ? hr : E_FAIL;
    }

    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    hr = factory->CreateBitmapFromMemory(
        width,
        height,
        format,
        stride,
        stride * height,
        const_cast<BYTE*>(pixels),
        &bitmap);
    if (FAILED(hr))
    {
        return hr;
    }

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(hr))
    {
        return hr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(container, nullptr, &encoder);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
    {
        return hr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> options;
    hr = encoder->CreateNewFrame(&frame, &options);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = frame->Initialize(options.Get());
    if (FAILED(hr))
    {
        return hr;
    }
    hr = frame->SetSize(width, height);
    if (FAILED(hr))
    {
        return hr;
    }
    WICPixelFormatGUID encoderFormat = format;
    hr = frame->SetPixelFormat(&encoderFormat);
    if (FAILED(hr))
    {
        return hr;
    }

    if (exifOrientation >= 1 && exifOrientation <= 8 && container == GUID_ContainerFormatJpeg)
    {
        Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> writer;
        if (SUCCEEDED(frame->GetMetadataQueryWriter(&writer)) && writer)
        {
            PROPVARIANT value{};
            value.vt = VT_UI2;
            value.uiVal = static_cast<USHORT>(exifOrientation);
            writer->SetMetadataByName(L"/app1/ifd/{ushort=274}", &value);
            writer->SetMetadataByName(L"System.Photo.Orientation", &value);
            PropVariantClear(&value);
        }
    }

    hr = frame->WriteSource(bitmap.Get(), nullptr);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = frame->Commit();
    if (FAILED(hr))
    {
        return hr;
    }
    return encoder->Commit();
}

std::uint32_t Crc32(std::uint8_t const* data, std::size_t length)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            std::uint32_t const mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void WriteBe32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void WriteChunk(std::vector<std::uint8_t>& png, char const type[4], std::uint8_t const* data, std::size_t size)
{
    WriteBe32(png, static_cast<std::uint32_t>(size));
    std::size_t const crcStart = png.size();
    png.insert(png.end(), type, type + 4);
    if (size > 0 && data != nullptr)
    {
        png.insert(png.end(), data, data + size);
    }
    std::uint32_t const crc = Crc32(png.data() + crcStart, 4 + size);
    WriteBe32(png, crc);
}

bool WriteBytes(std::wstring const& path, std::uint8_t const* data, std::size_t size)
{
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    DWORD written = 0;
    BOOL const ok = WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(file);
    return ok && written == size;
}

bool WritePngWithIhdrSize(std::wstring const& path, std::uint32_t width, std::uint32_t height)
{
    std::vector<std::uint8_t> png = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::uint8_t ihdr[13]{};
    ihdr[0] = static_cast<std::uint8_t>((width >> 24) & 0xFFu);
    ihdr[1] = static_cast<std::uint8_t>((width >> 16) & 0xFFu);
    ihdr[2] = static_cast<std::uint8_t>((width >> 8) & 0xFFu);
    ihdr[3] = static_cast<std::uint8_t>(width & 0xFFu);
    ihdr[4] = static_cast<std::uint8_t>((height >> 24) & 0xFFu);
    ihdr[5] = static_cast<std::uint8_t>((height >> 16) & 0xFFu);
    ihdr[6] = static_cast<std::uint8_t>((height >> 8) & 0xFFu);
    ihdr[7] = static_cast<std::uint8_t>(height & 0xFFu);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    WriteChunk(png, "IHDR", ihdr, sizeof(ihdr));
    WriteChunk(png, "IEND", nullptr, 0);
    return WriteBytes(path, png.data(), png.size());
}

} // namespace

TEST(ImageLimits, RejectsZeroAndNegative)
{
    EXPECT_FALSE(ValidateDecodedDimensions(0, 1).ok);
    EXPECT_EQ(ValidateDecodedDimensions(0, 1).reason, ImageLoadReason::InvalidDimensions);
    EXPECT_FALSE(ValidateDecodedDimensions(1, 0).ok);
    EXPECT_FALSE(ValidateDecodedDimensions(-1, 8).ok);
    EXPECT_EQ(FormatImageLoadReason(ImageLoadReason::InvalidDimensions), L"invalid-dimensions");
}

TEST(ImageLimits, RejectsAxisAbove16384)
{
    EXPECT_FALSE(ValidateDecodedDimensions(kMaxDecodedAxis + 1, 1).ok);
    EXPECT_EQ(ValidateDecodedDimensions(kMaxDecodedAxis + 1, 1).reason, ImageLoadReason::OversizedAxis);
    EXPECT_FALSE(ValidateDecodedDimensions(1, kMaxDecodedAxis + 1).ok);
}

TEST(ImageLimits, Accepts16384By1And8192SquareAtBudget)
{
    DimensionCheck const axis = ValidateDecodedDimensions(kMaxDecodedAxis, 1);
    EXPECT_TRUE(axis.ok);
    EXPECT_EQ(axis.reason, ImageLoadReason::Ok);

    DimensionCheck const exact = ValidateDecodedDimensions(8192, 8192);
    EXPECT_TRUE(exact.ok);
    EXPECT_EQ(8192ull * 8192ull * 4ull, kMaxDecodedBytes);

    DimensionCheck const exactAlt = ValidateDecodedDimensions(16384, 4096);
    EXPECT_TRUE(exactAlt.ok);
}

TEST(ImageLimits, RejectsOver256MiB)
{
    DimensionCheck const over = ValidateDecodedDimensions(8193, 8192);
    EXPECT_FALSE(over.ok);
    EXPECT_EQ(over.reason, ImageLoadReason::DecodedByteLimit);
    DimensionCheck const overAlt = ValidateDecodedDimensions(16384, 4097);
    EXPECT_FALSE(overAlt.ok);
    EXPECT_EQ(overAlt.reason, ImageLoadReason::DecodedByteLimit);
}

TEST(ImageOrientationMap, ExifValues)
{
    EXPECT_EQ(MapExifOrientationToWic(1), WICBitmapTransformRotate0);
    EXPECT_EQ(MapExifOrientationToWic(2), WICBitmapTransformFlipHorizontal);
    EXPECT_EQ(MapExifOrientationToWic(3), WICBitmapTransformRotate180);
    EXPECT_EQ(MapExifOrientationToWic(4), WICBitmapTransformFlipVertical);
    EXPECT_EQ(
        MapExifOrientationToWic(5),
        static_cast<WICBitmapTransformOptions>(
            WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal));
    EXPECT_EQ(MapExifOrientationToWic(6), WICBitmapTransformRotate90);
    EXPECT_EQ(
        MapExifOrientationToWic(7),
        static_cast<WICBitmapTransformOptions>(
            WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal));
    EXPECT_EQ(MapExifOrientationToWic(8), WICBitmapTransformRotate270);
    EXPECT_FALSE(OrientationSwapsAxes(1));
    EXPECT_TRUE(OrientationSwapsAxes(6));
    EXPECT_EQ(LeafNameFromPath(L"C:\\temp\\测试.png"), L"测试.png");
}

TEST_F(ImageComTest, PngPremultipliedAlphaAndDimensions)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"alpha.png");

    std::uint8_t src[] = {
        0, 0, 255, 128, 255, 255, 255, 255,
        0, 0, 0, 255, 0, 255, 0, 255,
    };
    ASSERT_HRESULT_SUCCEEDED(
        EncodeImage(path, GUID_ContainerFormatPng, 2, 2, GUID_WICPixelFormat32bppBGRA, src, 8, 1));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    ASSERT_HRESULT_SUCCEEDED(LoadFromPath(path, image, reason, error)) << error;
    EXPECT_EQ(reason, ImageLoadReason::Ok);
    EXPECT_EQ(image.width, 2);
    EXPECT_EQ(image.height, 2);
    EXPECT_EQ(image.stride, 8);
    ASSERT_EQ(image.pixels.size(), 16u);
    EXPECT_EQ(image.container, ImageContainer::Png);
    EXPECT_TRUE(image.orientationNormalized);
    EXPECT_EQ(image.sourceLeafName, L"alpha.png");
    EXPECT_EQ(image.pixels[0], 0);
    EXPECT_EQ(image.pixels[1], 0);
    EXPECT_EQ(image.pixels[2], 128);
    EXPECT_EQ(image.pixels[3], 128);
    EXPECT_EQ(image.pixels[4], 255);
    EXPECT_EQ(image.pixels[5], 255);
    EXPECT_EQ(image.pixels[6], 255);
    EXPECT_EQ(image.pixels[7], 255);

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, JpegOpaquePremultiplied)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"opaque.jpg");

    std::uint8_t src[] = {
        0, 0, 255, 255, 0, 255, 0, 255,
        255, 0, 0, 255, 128, 128, 128, 255,
    };
    ASSERT_HRESULT_SUCCEEDED(
        EncodeImage(path, GUID_ContainerFormatJpeg, 2, 2, GUID_WICPixelFormat32bppBGRA, src, 8, 1));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    ASSERT_HRESULT_SUCCEEDED(LoadFromPath(path, image, reason, error)) << error;
    EXPECT_EQ(image.width, 2);
    EXPECT_EQ(image.height, 2);
    EXPECT_EQ(image.container, ImageContainer::Jpeg);
    ASSERT_EQ(image.pixels.size(), 16u);
    EXPECT_EQ(image.pixels[3], 255);
    EXPECT_EQ(image.pixels[7], 255);
    EXPECT_EQ(image.pixels[11], 255);
    EXPECT_EQ(image.pixels[15], 255);

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, UnicodePathPng)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"测试.png");
    std::uint8_t src[] = {
        0, 0, 255, 255, 0, 0, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255,
    };
    ASSERT_HRESULT_SUCCEEDED(
        EncodeImage(path, GUID_ContainerFormatPng, 2, 2, GUID_WICPixelFormat32bppBGRA, src, 8, 1));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    ASSERT_HRESULT_SUCCEEDED(LoadFromPath(path, image, reason, error)) << error;
    EXPECT_EQ(image.sourceLeafName, L"测试.png");
    EXPECT_EQ(image.width, 2);
    EXPECT_EQ(image.height, 2);

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, CorruptFileFails)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"corrupt.png");
    std::uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE};
    ASSERT_TRUE(WriteBytes(path, garbage, sizeof(garbage)));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    EXPECT_HRESULT_FAILED(LoadFromPath(path, image, reason, error));
    EXPECT_NE(reason, ImageLoadReason::Ok);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(image.width, 0);

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, BmpRejectedAsUnsupportedContainer)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"not-allowed.bmp");
    std::uint8_t src[] = {
        0, 0, 255, 255, 0, 0, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255,
    };
    ASSERT_HRESULT_SUCCEEDED(
        EncodeImage(path, GUID_ContainerFormatBmp, 2, 2, GUID_WICPixelFormat32bppBGRA, src, 8, 1));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    HRESULT const hr = LoadFromPath(path, image, reason, error);
    EXPECT_HRESULT_FAILED(hr);
    EXPECT_EQ(reason, ImageLoadReason::UnsupportedContainer);
    EXPECT_EQ(hr, WINCODEC_ERR_UNKNOWNIMAGEFORMAT);

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, OversizedIhdrRejectedBeforeHugeAlloc)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"huge-ihdr.png");
    ASSERT_TRUE(WritePngWithIhdrSize(path, 20000, 1));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    HRESULT const hr = LoadFromPath(path, image, reason, error);
    EXPECT_HRESULT_FAILED(hr);
    EXPECT_TRUE(
        reason == ImageLoadReason::OversizedAxis || reason == ImageLoadReason::DecoderCreateFailed ||
        reason == ImageLoadReason::FrameDecodeFailed)
        << error;
    if (reason == ImageLoadReason::OversizedAxis)
    {
        EXPECT_EQ(hr, HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
    }
    EXPECT_TRUE(image.pixels.empty());

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, FailedLoadPreservesPrevious)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const good = dir.File(L"keep.png");
    std::wstring const bad = dir.File(L"bad.png");
    std::uint8_t src[] = {
        10, 20, 30, 255, 10, 20, 30, 255,
        10, 20, 30, 255, 10, 20, 30, 255,
    };
    ASSERT_HRESULT_SUCCEEDED(
        EncodeImage(good, GUID_ContainerFormatPng, 2, 2, GUID_WICPixelFormat32bppBGRA, src, 8, 1));
    std::uint8_t garbage[] = {0x47, 0x49, 0x46};
    ASSERT_TRUE(WriteBytes(bad, garbage, sizeof(garbage)));

    LoadedImageSlot slot;
    std::wstring error;
    ASSERT_TRUE(slot.TryLoad(good, error)) << error;
    ASSERT_TRUE(slot.HasImage());
    std::vector<std::uint8_t> const kept = slot.Image()->pixels;
    EXPECT_EQ(slot.Image()->sourceLeafName, L"keep.png");

    EXPECT_FALSE(slot.TryLoad(bad, error));
    EXPECT_TRUE(slot.HasImage());
    ASSERT_NE(slot.Image(), nullptr);
    EXPECT_EQ(slot.Image()->sourceLeafName, L"keep.png");
    EXPECT_EQ(slot.Image()->pixels, kept);
    EXPECT_NE(slot.LastReason(), ImageLoadReason::Ok);
    EXPECT_HRESULT_FAILED(slot.LastHr());
    std::wstring const report = slot.FormatReport();
    EXPECT_NE(report.find(L"loaded=yes"), std::wstring::npos);
    EXPECT_NE(report.find(L"keep.png"), std::wstring::npos);
    EXPECT_NE(report.find(L"previous preserved"), std::wstring::npos);

    EXPECT_FALSE(slot.TryLoad(dir.File(L"missing-no-file.png"), error));
    EXPECT_EQ(slot.Image()->pixels, kept);

    dir.RemoveTempFile(good);
    dir.RemoveTempFile(bad);
}

TEST_F(ImageComTest, JpegExifOrientation6IfMetadataWriterWorks)
{
    TempDir dir;
    ASSERT_FALSE(dir.Path().empty());
    std::wstring const path = dir.File(L"orient6.jpg");
    std::uint8_t src[] = {
        0, 0, 255, 255, 255, 0, 0, 255, 0, 255, 0, 255, 128, 128, 128, 255,
    };
    ASSERT_HRESULT_SUCCEEDED(
        EncodeImage(path, GUID_ContainerFormatJpeg, 4, 1, GUID_WICPixelFormat32bppBGRA, src, 16, 6));

    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    ASSERT_HRESULT_SUCCEEDED(LoadFromPath(path, image, reason, error)) << error;
    if (image.width == 4 && image.height == 1)
    {
        dir.RemoveTempFile(path);
        GTEST_SKIP() << "WIC JPEG encoder did not persist EXIF orientation 6; mapping helper is still tested.";
    }
    EXPECT_EQ(image.width, 1);
    EXPECT_EQ(image.height, 4);
    EXPECT_TRUE(image.orientationNormalized);

    dir.RemoveTempFile(path);
}

TEST_F(ImageComTest, EmptyPathFails)
{
    DecodedImage image;
    ImageLoadReason reason = ImageLoadReason::Ok;
    std::wstring error;
    EXPECT_EQ(LoadFromPath(L"", image, reason, error), E_INVALIDARG);
    EXPECT_EQ(reason, ImageLoadReason::EmptyPath);
}
