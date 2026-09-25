#include <gtest/gtest.h>

#include "capture/RoiReadback.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using tracing::capture::DefaultRoiDownsample;
using tracing::capture::FormatRoiClipReject;
using tracing::capture::FormatRoiHandoffAction;
using tracing::capture::FormatRoiKind;
using tracing::capture::FormatRoiReadbackReport;
using tracing::capture::kMaxRoiBytes;
using tracing::capture::kRoiBgraBytesPerPixel;
using tracing::capture::kRoiDownsampleMaxAxis;
using tracing::capture::PackBgraRows;
using tracing::capture::RoiBufferMeta;
using tracing::capture::RoiClipReject;
using tracing::capture::RoiClipRequest;
using tracing::capture::RoiClipResult;
using tracing::capture::RoiHandoffAction;
using tracing::capture::RoiKind;
using tracing::capture::RoiPixelRect;
using tracing::capture::RoiReadbackDiagnostics;
using tracing::capture::RoiReadbackPolicy;
using tracing::capture::TryClipRoi;

RoiClipRequest MakeRequest(
    RoiKind kind,
    int sourceW,
    int sourceH,
    int x,
    int y,
    int w,
    int h,
    int downsample = 1)
{
    RoiClipRequest request{};
    request.kind = kind;
    request.sourceWidth = sourceW;
    request.sourceHeight = sourceH;
    request.requested.x = x;
    request.requested.y = y;
    request.requested.w = w;
    request.requested.h = h;
    request.downsample = downsample;
    return request;
}

RoiBufferMeta MakeMeta(
    std::uint64_t sequence,
    std::int64_t ticks,
    std::uint64_t targetGeneration,
    std::uint64_t geometryGeneration)
{
    RoiBufferMeta meta{};
    meta.sequence = sequence;
    meta.captureTicks = ticks;
    meta.targetGeneration = targetGeneration;
    meta.geometryGeneration = geometryGeneration;
    meta.kind = RoiKind::Canvas;
    return meta;
}

} // namespace

TEST(RoiClip, CanvasInteriorAccepted)
{
    RoiClipResult const result = TryClipRoi(MakeRequest(RoiKind::Canvas, 1920, 1080, 80, 80, 640, 480));
    EXPECT_EQ(result.reason, RoiClipReject::Ok);
    EXPECT_EQ(result.kind, RoiKind::Canvas);
    EXPECT_EQ(result.clipped.x, 80);
    EXPECT_EQ(result.clipped.y, 80);
    EXPECT_EQ(result.clipped.w, 640);
    EXPECT_EQ(result.clipped.h, 480);
    EXPECT_EQ(result.outWidth, 640);
    EXPECT_EQ(result.outHeight, 480);
    EXPECT_EQ(result.destStride, 640 * kRoiBgraBytesPerPixel);
    EXPECT_EQ(result.destBytes, 640ull * 480ull * 4ull);
}

TEST(RoiClip, NavigatorIndependentlyClipped)
{
    RoiClipResult const canvas = TryClipRoi(MakeRequest(RoiKind::Canvas, 800, 600, 10, 10, 100, 80));
    RoiClipResult const navigator =
        TryClipRoi(MakeRequest(RoiKind::Navigator, 800, 600, 700, 500, 200, 200));
    EXPECT_EQ(canvas.reason, RoiClipReject::Ok);
    EXPECT_EQ(canvas.kind, RoiKind::Canvas);
    EXPECT_EQ(navigator.reason, RoiClipReject::Ok);
    EXPECT_EQ(navigator.kind, RoiKind::Navigator);
    EXPECT_EQ(navigator.clipped.x, 700);
    EXPECT_EQ(navigator.clipped.y, 500);
    EXPECT_EQ(navigator.clipped.w, 100);
    EXPECT_EQ(navigator.clipped.h, 100);
}

TEST(RoiClip, RejectsNonPositiveAndFullyOutside)
{
    EXPECT_EQ(
        TryClipRoi(MakeRequest(RoiKind::Canvas, 100, 100, 0, 0, 0, 10)).reason,
        RoiClipReject::NonPositiveSize);
    EXPECT_EQ(
        TryClipRoi(MakeRequest(RoiKind::Canvas, 100, 100, 0, 0, 10, -4)).reason,
        RoiClipReject::NonPositiveSize);
    EXPECT_EQ(
        TryClipRoi(MakeRequest(RoiKind::Canvas, 100, 50, 1000, 0, 10, 10)).reason,
        RoiClipReject::EmptyAfterClip);
    EXPECT_EQ(
        TryClipRoi(MakeRequest(RoiKind::Navigator, 64, 64, -40, -40, 10, 10)).reason,
        RoiClipReject::EmptyAfterClip);
}

TEST(RoiClip, PartialOutOfBoundsClipped)
{
    RoiClipResult const result = TryClipRoi(MakeRequest(RoiKind::Canvas, 100, 80, -10, 70, 50, 30));
    EXPECT_EQ(result.reason, RoiClipReject::Ok);
    EXPECT_EQ(result.clipped.x, 0);
    EXPECT_EQ(result.clipped.y, 70);
    EXPECT_EQ(result.clipped.w, 40);
    EXPECT_EQ(result.clipped.h, 10);
}

TEST(RoiClip, DownsampleAndOddFloor)
{
    RoiClipResult const even =
        TryClipRoi(MakeRequest(RoiKind::Canvas, 640, 480, 0, 0, 640, 480, 2));
    EXPECT_EQ(even.reason, RoiClipReject::Ok);
    EXPECT_EQ(even.outWidth, 320);
    EXPECT_EQ(even.outHeight, 240);
    EXPECT_EQ(even.downsample, 2);
    EXPECT_EQ(even.destStride, 320 * 4);

    RoiClipResult const odd = TryClipRoi(MakeRequest(RoiKind::Canvas, 5, 5, 0, 0, 5, 5, 2));
    EXPECT_EQ(odd.reason, RoiClipReject::Ok);
    EXPECT_EQ(odd.outWidth, 2);
    EXPECT_EQ(odd.outHeight, 2);

    EXPECT_EQ(
        TryClipRoi(MakeRequest(RoiKind::Canvas, 3, 3, 0, 0, 3, 3, 4)).reason,
        RoiClipReject::OutputEmpty);
    EXPECT_EQ(
        TryClipRoi(MakeRequest(RoiKind::Canvas, 64, 64, 0, 0, 32, 32, 0)).reason,
        RoiClipReject::BadDownsample);
}

TEST(RoiClip, ByteLimitAndOverflowReject)
{
    int const side = 4097;
    RoiClipResult const oversize =
        TryClipRoi(MakeRequest(RoiKind::Canvas, side, side, 0, 0, side, side));
    EXPECT_EQ(oversize.reason, RoiClipReject::ByteLimit);
    EXPECT_GT(static_cast<std::uint64_t>(side) * static_cast<std::uint64_t>(side) * 4ull, kMaxRoiBytes);

    RoiClipResult const axis = TryClipRoi(MakeRequest(RoiKind::Canvas, 20000, 16, 0, 0, 20000, 16));
    EXPECT_EQ(axis.reason, RoiClipReject::OversizedAxis);
}

TEST(RoiDownsample, DefaultBoundedAxis)
{
    EXPECT_EQ(DefaultRoiDownsample(640, 480), 1);
    EXPECT_EQ(DefaultRoiDownsample(kRoiDownsampleMaxAxis, kRoiDownsampleMaxAxis), 1);
    EXPECT_EQ(DefaultRoiDownsample(1025, 10), 2);
    EXPECT_EQ(DefaultRoiDownsample(2576, 1440), 3);
}

TEST(RoiPack, RespectsSourcePitchAndKnownPixel)
{
    constexpr int kSrcW = 50;
    constexpr int kSrcH = 4;
    constexpr int kSrcPitch = 256;
    constexpr int kDstPitch = kSrcW * kRoiBgraBytesPerPixel;
    std::vector<std::uint8_t> src(static_cast<std::size_t>(kSrcPitch) * kSrcH, 0);
    int const px = 10;
    int const py = 2;
    src[static_cast<std::size_t>(py) * kSrcPitch + static_cast<std::size_t>(px) * 4 + 0] = 11;
    src[static_cast<std::size_t>(py) * kSrcPitch + static_cast<std::size_t>(px) * 4 + 1] = 22;
    src[static_cast<std::size_t>(py) * kSrcPitch + static_cast<std::size_t>(px) * 4 + 2] = 33;
    src[static_cast<std::size_t>(py) * kSrcPitch + static_cast<std::size_t>(px) * 4 + 3] = 44;

    std::vector<std::uint8_t> dst(static_cast<std::size_t>(kDstPitch) * kSrcH, 0);
    ASSERT_TRUE(PackBgraRows(src.data(), kSrcPitch, dst.data(), kDstPitch, kSrcW, kSrcH, 1));
    std::size_t const destIndex =
        static_cast<std::size_t>(py) * kDstPitch + static_cast<std::size_t>(px) * 4;
    EXPECT_EQ(kDstPitch, 200);
    EXPECT_EQ(dst[destIndex + 0], 11);
    EXPECT_EQ(dst[destIndex + 1], 22);
    EXPECT_EQ(dst[destIndex + 2], 33);
    EXPECT_EQ(dst[destIndex + 3], 44);
}

TEST(RoiPack, PointSampleDownsample)
{
    constexpr int kSrcW = 4;
    constexpr int kSrcH = 4;
    constexpr int kPitch = kSrcW * kRoiBgraBytesPerPixel;
    std::vector<std::uint8_t> src(static_cast<std::size_t>(kPitch) * kSrcH, 0);
    auto put = [&](int x, int y, std::uint8_t b) {
        src[static_cast<std::size_t>(y) * kPitch + static_cast<std::size_t>(x) * 4] = b;
    };
    put(0, 0, 1);
    put(2, 0, 2);
    put(0, 2, 3);
    put(2, 2, 4);
    put(1, 1, 99);

    std::vector<std::uint8_t> dst(2 * 2 * 4, 0);
    ASSERT_TRUE(PackBgraRows(src.data(), kPitch, dst.data(), 8, kSrcW, kSrcH, 2));
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[4], 2);
    EXPECT_EQ(dst[8], 3);
    EXPECT_EQ(dst[12], 4);
}

TEST(RoiPending, ThirdEnqueueDropsOldestAndKeepsMetadata)
{
    RoiReadbackPolicy policy;
    RoiHandoffAction const first = policy.Arrive(MakeMeta(1, 100, 7, 11)).action;
    RoiHandoffAction const second = policy.Arrive(MakeMeta(2, 200, 7, 11)).action;
    EXPECT_EQ(first, RoiHandoffAction::Enqueue);
    EXPECT_EQ(second, RoiHandoffAction::Enqueue);
    EXPECT_EQ(policy.Pending(), 2u);

    auto const third = policy.Arrive(MakeMeta(3, 300, 8, 12));
    EXPECT_EQ(third.action, RoiHandoffAction::DropOldestThenEnqueue);
    EXPECT_TRUE(third.droppedValid);
    EXPECT_EQ(third.dropped.sequence, 1u);
    EXPECT_EQ(third.dropped.captureTicks, 100);
    EXPECT_EQ(third.dropped.targetGeneration, 7u);
    EXPECT_EQ(third.dropped.geometryGeneration, 11u);
    EXPECT_EQ(policy.Pending(), 2u);
    EXPECT_EQ(policy.Dropped(), 1u);
    EXPECT_EQ(policy.Oldest().sequence, 2u);
    EXPECT_EQ(policy.Oldest().captureTicks, 200);
    EXPECT_EQ(policy.Newest().sequence, 3u);
    EXPECT_EQ(policy.Newest().targetGeneration, 8u);
    EXPECT_EQ(policy.Newest().geometryGeneration, 12u);

    policy.NoteCompleted();
    EXPECT_EQ(policy.Pending(), 1u);
    EXPECT_EQ(policy.Completed(), 1u);
    EXPECT_EQ(policy.Oldest().sequence, 3u);
}

TEST(RoiPending, DiscardAllCountsDropped)
{
    RoiReadbackPolicy policy;
    policy.Arrive(MakeMeta(4, 1, 1, 1));
    policy.Arrive(MakeMeta(5, 2, 1, 1));
    policy.DiscardAll();
    EXPECT_EQ(policy.Pending(), 0u);
    EXPECT_EQ(policy.Dropped(), 2u);
}

TEST(RoiFormat, LabelsAndReportMentionNotZeroCopy)
{
    EXPECT_STREQ(FormatRoiKind(RoiKind::Canvas), "canvas");
    EXPECT_STREQ(FormatRoiKind(RoiKind::Navigator), "navigator");
    EXPECT_STREQ(FormatRoiClipReject(RoiClipReject::EmptyAfterClip), "empty-after-clip");
    EXPECT_STREQ(FormatRoiHandoffAction(RoiHandoffAction::DropOldestThenEnqueue), "drop-oldest-then-enqueue");

    RoiReadbackDiagnostics diagnostics{};
    diagnostics.kind = RoiKind::Canvas;
    diagnostics.zeroCopy = true;
    std::wstring const report = FormatRoiReadbackReport(diagnostics);
    EXPECT_NE(report.find(L"zeroCopy=no"), std::wstring::npos);
    EXPECT_EQ(report.find(L"zeroCopy=yes"), std::wstring::npos);
}
