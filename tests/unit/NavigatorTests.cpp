#include <gtest/gtest.h>

#include "tracking/NavigatorObserver.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using tracing::core::DocumentScaleLabel;
using tracing::core::Rect2;
using tracing::core::RoiRejectReason;
using tracing::tracking::FormatNavigatorObservation;
using tracing::tracking::FormatNavigatorReject;
using tracing::tracking::FormatNavigatorSource;
using tracing::tracking::NavigatorObserver;
using tracing::tracking::NavigatorObservation;
using tracing::tracking::NavigatorPixelFormat;
using tracing::tracking::NavigatorReject;
using tracing::tracking::NavigatorSource;
using tracing::tracking::NavigatorView;

constexpr int kWidth = 160;
constexpr int kHeight = 120;
constexpr double kPi = 3.14159265358979323846;

cv::Mat MakeTexturedGray(std::uint32_t seed)
{
    cv::Mat gray(kHeight, kWidth, CV_8UC1);
    std::uint32_t rng = seed;
    auto next = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return rng;
    };
    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            std::uint32_t const noise = next() >> 24;
            int value = 110 + static_cast<int>(noise % 50);
            gray.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(value);
        }
    }
    return gray;
}

void FillRotatedRect(cv::Mat& gray, cv::Point2f center, cv::Size2f size, float angleDeg, std::uint8_t fill)
{
    cv::RotatedRect const box(center, size, angleDeg);
    cv::Point2f pts[4]{};
    box.points(pts);
    cv::Point vertices[4]{};
    for (int i = 0; i < 4; ++i)
    {
        vertices[i] = pts[i];
    }
    cv::fillConvexPoly(gray, vertices, 4, cv::Scalar(fill));
    std::vector<cv::Point> outline(vertices, vertices + 4);
    cv::polylines(gray, outline, true, cv::Scalar(fill < 128 ? 255 : 0), 2, cv::LINE_AA);
}

NavigatorView GrayView(
    cv::Mat const& gray,
    std::uint64_t sequence = 1,
    std::int64_t ticks = 1000)
{
    NavigatorView view{};
    view.data = gray.data;
    view.width = gray.cols;
    view.height = gray.rows;
    view.stride = static_cast<int>(gray.step);
    view.format = NavigatorPixelFormat::Gray8;
    view.sequence = sequence;
    view.captureTicks = ticks;
    view.targetGeneration = 7;
    view.geometryGeneration = 3;
    return view;
}

bool ApplyDefaultRoi(NavigatorObserver& observer)
{
    RoiRejectReason reason = RoiRejectReason::Ok;
    Rect2 roi{};
    roi.x = 10.0;
    roi.y = 20.0;
    roi.width = 160.0;
    roi.height = 120.0;
    return observer.SetRoi(roi, 1920.0, 1080.0, reason);
}

NavigatorObservation ObserveRect(
    NavigatorObserver& observer,
    cv::Point2f center,
    cv::Size2f size,
    float angleDeg,
    std::uint64_t sequence)
{
    cv::Mat gray = MakeTexturedGray(0xC0FFEEu + static_cast<std::uint32_t>(sequence));
    FillRotatedRect(gray, center, size, angleDeg, 18);
    return observer.Observe(GrayView(gray, sequence, static_cast<std::int64_t>(sequence) * 100));
}

double WrapHalfPi(double radians)
{
    while (radians > kPi * 0.5)
    {
        radians -= kPi;
    }
    while (radians <= -kPi * 0.5)
    {
        radians += kPi;
    }
    return radians;
}

} // namespace

TEST(NavigatorObserver, DisableIgnoresPixels)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    observer.Disable();

    cv::Mat gray = MakeTexturedGray(1);
    FillRotatedRect(gray, cv::Point2f(80.f, 60.f), cv::Size2f(50.f, 30.f), 0.f, 18);
    NavigatorObservation const observation = observer.Observe(GrayView(gray, 9, 99));
    EXPECT_EQ(observation.source, NavigatorSource::Disabled);
    EXPECT_EQ(observation.reject, NavigatorReject::Disabled);
    EXPECT_FALSE(observation.hasTranslation);
    EXPECT_FALSE(observation.hasRelativeScale);
    EXPECT_FALSE(observation.hasRotation);
    EXPECT_FALSE(observation.hasParity);
    EXPECT_FALSE(observation.hasDocumentPosition);
    EXPECT_FALSE(observation.hasZoomPercent);
    EXPECT_EQ(observation.sequence, 9u);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_FALSE(observer.RoiApplied());
}

TEST(NavigatorObserver, DefaultDisabledWithoutRoi)
{
    NavigatorObserver observer;
    cv::Mat gray = MakeTexturedGray(2);
    FillRotatedRect(gray, cv::Point2f(80.f, 60.f), cv::Size2f(50.f, 30.f), 0.f, 18);
    NavigatorObservation const observation = observer.Observe(GrayView(gray));
    EXPECT_EQ(observation.source, NavigatorSource::Disabled);
    EXPECT_EQ(observation.reject, NavigatorReject::Disabled);
    EXPECT_FALSE(observation.hasTranslation);
    EXPECT_FALSE(observation.hasDocumentPosition);
}

TEST(NavigatorObserver, RejectsInvalidRoi)
{
    NavigatorObserver observer;
    RoiRejectReason reason = RoiRejectReason::Ok;
    Rect2 roi{};
    roi.x = 10.0;
    roi.y = 10.0;
    roi.width = 4000.0;
    roi.height = 10.0;
    EXPECT_FALSE(observer.SetRoi(roi, 1920.0, 1080.0, reason));
    EXPECT_EQ(reason, RoiRejectReason::OutOfClient);
    EXPECT_FALSE(observer.RoiApplied());
}

TEST(NavigatorObserver, MissingOnBlankAndEmptyView)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));

    cv::Mat blank(kHeight, kWidth, CV_8UC1, cv::Scalar(128));
    NavigatorObservation const blankObs = observer.Observe(GrayView(blank, 2, 200));
    EXPECT_EQ(blankObs.source, NavigatorSource::Missing);
    EXPECT_EQ(blankObs.reject, NavigatorReject::BlankOrNoIndicator);
    EXPECT_FALSE(blankObs.hasTranslation);

    NavigatorView empty{};
    empty.sequence = 3;
    empty.captureTicks = 300;
    NavigatorObservation const emptyObs = observer.Observe(empty);
    EXPECT_EQ(emptyObs.source, NavigatorSource::Missing);
    EXPECT_EQ(emptyObs.reject, NavigatorReject::DegenerateSize);
    EXPECT_FALSE(emptyObs.hasTranslation);
}

TEST(NavigatorObserver, MissingOnTextureWithoutIndicator)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    cv::Mat gray = MakeTexturedGray(3);
    NavigatorObservation const observation = observer.Observe(GrayView(gray, 4, 400));
    EXPECT_EQ(observation.source, NavigatorSource::Missing);
    EXPECT_FALSE(observation.hasTranslation);
    EXPECT_FALSE(observation.hasRelativeScale);
    EXPECT_FALSE(observation.hasZoomPercent);
}

TEST(NavigatorObserver, PanChangesTranslationOnlyInThumbnail)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));

    NavigatorObservation const a =
        ObserveRect(observer, cv::Point2f(50.f, 40.f), cv::Size2f(48.f, 28.f), 0.f, 10);
    NavigatorObservation const b =
        ObserveRect(observer, cv::Point2f(110.f, 70.f), cv::Size2f(48.f, 28.f), 0.f, 11);

    ASSERT_EQ(a.source, NavigatorSource::Observed) << FormatNavigatorObservation(a);
    ASSERT_EQ(b.source, NavigatorSource::Observed) << FormatNavigatorObservation(b);
    EXPECT_TRUE(a.hasTranslation);
    EXPECT_TRUE(a.hasRelativeScale);
    EXPECT_FALSE(a.hasParity);
    EXPECT_FALSE(a.hasDocumentPosition);
    EXPECT_FALSE(a.hasZoomPercent);
    EXPECT_GT(b.centerU - a.centerU, 0.25);
    EXPECT_GT(b.centerV - a.centerV, 0.15);
    EXPECT_NEAR(a.centerU, 50.0 / kWidth, 0.08);
    EXPECT_NEAR(b.centerU, 110.0 / kWidth, 0.08);
    EXPECT_LT(std::fabs(a.sizeU - b.sizeU), 0.08);
}

TEST(NavigatorObserver, RelativeScaleDoesNotClaimZoomPercent)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));

    NavigatorObservation const small =
        ObserveRect(observer, cv::Point2f(80.f, 60.f), cv::Size2f(32.f, 20.f), 0.f, 20);
    NavigatorObservation const large =
        ObserveRect(observer, cv::Point2f(80.f, 60.f), cv::Size2f(80.f, 50.f), 0.f, 21);

    ASSERT_EQ(small.source, NavigatorSource::Observed) << FormatNavigatorObservation(small);
    ASSERT_EQ(large.source, NavigatorSource::Observed) << FormatNavigatorObservation(large);
    EXPECT_TRUE(small.hasRelativeScale);
    EXPECT_FALSE(small.hasZoomPercent);
    EXPECT_FALSE(large.hasZoomPercent);
    EXPECT_GT(large.sizeU, small.sizeU + 0.12);
    std::string const text = FormatNavigatorObservation(small);
    EXPECT_NE(text.find("zoom%=no"), std::string::npos);
    EXPECT_NE(text.find("not CSP"), std::string::npos);
}

TEST(NavigatorObserver, RotationWhenUniqueNonSquare)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    float const angleDeg = 30.f;
    NavigatorObservation const observation =
        ObserveRect(observer, cv::Point2f(80.f, 60.f), cv::Size2f(70.f, 24.f), angleDeg, 30);

    ASSERT_EQ(observation.source, NavigatorSource::Observed) << FormatNavigatorObservation(observation);
    EXPECT_TRUE(observation.hasRotation);
    double const expected = WrapHalfPi(angleDeg * kPi / 180.0);
    double delta = std::fabs(WrapHalfPi(observation.radiansClockwise - expected));
    EXPECT_LT(delta, 0.20) << "got " << observation.radiansClockwise << " expected " << expected;
}

TEST(NavigatorObserver, OccludedWhenIndicatorHitsBorder)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    NavigatorObservation const observation =
        ObserveRect(observer, cv::Point2f(8.f, 60.f), cv::Size2f(50.f, 30.f), 0.f, 40);
    EXPECT_EQ(observation.source, NavigatorSource::Occluded) << FormatNavigatorObservation(observation);
    EXPECT_EQ(observation.reject, NavigatorReject::Occluded);
    EXPECT_FALSE(observation.hasTranslation);
    EXPECT_FALSE(observation.hasDocumentPosition);
    EXPECT_LT(observation.confidence, 0.5);
}

TEST(NavigatorObserver, AmbiguousWhenTwoSimilarIndicators)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    cv::Mat gray = MakeTexturedGray(5);
    FillRotatedRect(gray, cv::Point2f(45.f, 40.f), cv::Size2f(44.f, 26.f), 0.f, 18);
    FillRotatedRect(gray, cv::Point2f(115.f, 80.f), cv::Size2f(44.f, 26.f), 0.f, 18);
    NavigatorObservation const observation = observer.Observe(GrayView(gray, 50, 5000));
    EXPECT_EQ(observation.source, NavigatorSource::Ambiguous) << FormatNavigatorObservation(observation);
    EXPECT_EQ(observation.reject, NavigatorReject::Ambiguous);
    EXPECT_FALSE(observation.hasTranslation);
    EXPECT_GE(observation.candidateCount, 2);
}

TEST(NavigatorObserver, MappingFillsDocumentNotUncalibratedInventedCoords)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));

    NavigatorObservation const uncalibrated =
        ObserveRect(observer, cv::Point2f(40.f, 30.f), cv::Size2f(40.f, 24.f), 0.f, 60);
    ASSERT_EQ(uncalibrated.source, NavigatorSource::Observed);
    EXPECT_FALSE(uncalibrated.hasDocumentPosition);
    EXPECT_FALSE(uncalibrated.hasZoomPercent);

    observer.SetMapping(1000.0, 800.0);
    NavigatorObservation const calibrated =
        ObserveRect(observer, cv::Point2f(40.f, 30.f), cv::Size2f(40.f, 24.f), 0.f, 61);
    ASSERT_EQ(calibrated.source, NavigatorSource::Observed) << FormatNavigatorObservation(calibrated);
    EXPECT_TRUE(calibrated.hasDocumentPosition);
    EXPECT_NEAR(calibrated.documentX, calibrated.centerU * 1000.0, 1.0e-6);
    EXPECT_NEAR(calibrated.documentY, calibrated.centerV * 800.0, 1.0e-6);
    EXPECT_EQ(calibrated.mapping.units, DocumentScaleLabel::DeclaredDocumentPixels);
    EXPECT_NE(FormatNavigatorObservation(calibrated).find("not CSP %"), std::string::npos);
}

TEST(NavigatorObserver, ReapplyRoiReenablesAfterDisable)
{
    NavigatorObserver observer;
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    observer.Disable();
    ASSERT_TRUE(ApplyDefaultRoi(observer));
    EXPECT_TRUE(observer.Enabled());
    EXPECT_TRUE(observer.RoiApplied());
    NavigatorObservation const observation =
        ObserveRect(observer, cv::Point2f(80.f, 60.f), cv::Size2f(48.f, 28.f), 0.f, 70);
    EXPECT_EQ(observation.source, NavigatorSource::Observed) << FormatNavigatorObservation(observation);
}

TEST(NavigatorObserver, FormatNamesAreStable)
{
    EXPECT_STREQ(FormatNavigatorSource(NavigatorSource::Disabled), "disabled");
    EXPECT_STREQ(FormatNavigatorSource(NavigatorSource::Missing), "missing");
    EXPECT_STREQ(FormatNavigatorReject(NavigatorReject::NoRoi), "no-roi");
    EXPECT_STREQ(FormatNavigatorReject(NavigatorReject::BlankOrNoIndicator), "blank-or-no-indicator");
}

TEST(NavigatorObserver, RealCspFixtureIsLocalOptIn)
{
    char* path = nullptr;
#ifdef _MSC_VER
    size_t length = 0;
    if (_dupenv_s(&path, &length, "TRACING_NAVIGATOR_FIXTURE") != 0)
    {
        path = nullptr;
    }
#else
    path = std::getenv("TRACING_NAVIGATOR_FIXTURE");
#endif
    if (path == nullptr || path[0] == '\0')
    {
#ifdef _MSC_VER
        std::free(path);
#endif
        GTEST_SKIP() << "TRACING_NAVIGATOR_FIXTURE unset; real CSP Navigator pixels are local "
                        "opt-in evidence and must not be committed";
    }
    std::string const fixture(path);
#ifdef _MSC_VER
    std::free(path);
#endif
    GTEST_SKIP() << "opencv imgcodecs not linked this step; fixture path is local-only: " << fixture;
}
