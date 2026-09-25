#include <gtest/gtest.h>

#include "core/Transform2D.h"
#include "tracking/VisualTracker.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

namespace {

using tracing::core::Apply;
using tracing::core::Compose;
using tracing::core::Flip;
using tracing::core::RotateAbout;
using tracing::core::ScaleAbout;
using tracing::core::Space;
using tracing::core::Transform2D;
using tracing::core::Translate;
using tracing::core::Vec2;
using tracing::tracking::CanvasView;
using tracing::tracking::Correspondence;
using tracing::tracking::EstimateCanvasMotion;
using tracing::tracking::EstimateFromCorrespondences;
using tracing::tracking::PixelFormat;
using tracing::tracking::VisualEstimate;
using tracing::tracking::VisualReject;
using tracing::tracking::VisualRejectName;
using tracing::tracking::VisualTrackerOptions;

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr double kPi = std::numbers::pi_v<double>;

CanvasView GrayView(cv::Mat const& gray)
{
    CanvasView view{};
    view.data = gray.data;
    view.width = gray.cols;
    view.height = gray.rows;
    view.stride = static_cast<int>(gray.step);
    view.format = PixelFormat::Gray8;
    return view;
}

CanvasView BgraView(cv::Mat const& bgra)
{
    CanvasView view{};
    view.data = bgra.data;
    view.width = bgra.cols;
    view.height = bgra.rows;
    view.stride = static_cast<int>(bgra.step);
    view.format = PixelFormat::Bgra32;
    return view;
}

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
            int const checker = ((x / 10) ^ (y / 10)) & 1;
            int const value = static_cast<int>(noise) + checker * 70 + ((x * 17 + y * 11) & 47);
            gray.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(std::clamp(value, 0, 255));
        }
    }
    cv::circle(gray, cv::Point(48, 42), 18, cv::Scalar(255), cv::FILLED);
    cv::circle(gray, cv::Point(kWidth - 56, 46), 14, cv::Scalar(16), cv::FILLED);
    cv::rectangle(gray, cv::Point(28, kHeight - 72), cv::Point(96, kHeight - 22), cv::Scalar(230),
                  cv::FILLED);
    cv::rectangle(gray, cv::Point(150, 96), cv::Point(214, 110), cv::Scalar(8), cv::FILLED);
    cv::rectangle(gray, cv::Point(150, 96), cv::Point(164, 176), cv::Scalar(8), cv::FILLED);
    cv::line(gray, cv::Point(18, 18), cv::Point(kWidth - 24, 78), cv::Scalar(250), 3);
    cv::line(gray, cv::Point(40, kHeight - 18), cv::Point(kWidth - 30, kHeight - 90), cv::Scalar(12),
             3);
    return gray;
}

cv::Mat ToOpenCvAffine(Transform2D const& transform)
{
    cv::Mat matrix(2, 3, CV_64F);
    matrix.at<double>(0, 0) = transform.matrix.m[0];
    matrix.at<double>(0, 1) = transform.matrix.m[3];
    matrix.at<double>(0, 2) = transform.matrix.m[6];
    matrix.at<double>(1, 0) = transform.matrix.m[1];
    matrix.at<double>(1, 1) = transform.matrix.m[4];
    matrix.at<double>(1, 2) = transform.matrix.m[7];
    return matrix;
}

cv::Mat WarpGray(cv::Mat const& source, Transform2D const& transform)
{
    cv::Mat destination;
    cv::warpAffine(source, destination, ToOpenCvAffine(transform), source.size(), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar(128));
    return destination;
}

cv::Mat WarpGrayMat(cv::Mat const& source, cv::Mat const& affine)
{
    cv::Mat destination;
    cv::warpAffine(source, destination, affine, source.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                   cv::Scalar(128));
    return destination;
}

Transform2D MustCompose(Transform2D const& first, Transform2D const& then)
{
    std::optional<Transform2D> const composed = Compose(first, then);
    EXPECT_TRUE(composed.has_value());
    return composed.value_or(first);
}

Vec2 MustMap(Transform2D const& transform, Vec2 point)
{
    std::optional<Vec2> const mapped = Apply(transform, point);
    EXPECT_TRUE(mapped.has_value());
    return mapped.value_or(point);
}

void DecomposeExpected(
    Transform2D const& transform,
    double& tx,
    double& ty,
    double& scale,
    double& radians)
{
    double const a = transform.matrix.m[0];
    double const c = transform.matrix.m[1];
    tx = transform.matrix.m[6];
    ty = transform.matrix.m[7];
    scale = std::hypot(a, c);
    radians = std::atan2(c, a);
}

std::vector<Correspondence> GridCorrespondences(Transform2D const& transform, int step = 24)
{
    std::vector<Correspondence> points;
    for (int y = 24; y < kHeight - 24; y += step)
    {
        for (int x = 24; x < kWidth - 24; x += step)
        {
            Vec2 const mapped = MustMap(transform, Vec2{static_cast<double>(x), static_cast<double>(y)});
            Correspondence item{};
            item.priorX = static_cast<double>(x);
            item.priorY = static_cast<double>(y);
            item.currentX = mapped.x;
            item.currentY = mapped.y;
            points.push_back(item);
        }
    }
    return points;
}

void PrintErrors(
    char const* label,
    VisualEstimate const& estimate,
    double expectedTx,
    double expectedTy,
    double expectedScale,
    double expectedRadians)
{
    double const txErr = std::abs(estimate.tx - expectedTx);
    double const tyErr = std::abs(estimate.ty - expectedTy);
    double const scaleErr = std::abs(estimate.uniformScale - expectedScale);
    double const degErr = std::abs(estimate.radiansClockwise - expectedRadians) * 180.0 / kPi;
    std::cout << label << " reject=" << VisualRejectName(estimate.reject)
              << " txErr=" << txErr << " tyErr=" << tyErr << " scaleErr=" << scaleErr
              << " degErr=" << degErr << " inliers=" << estimate.inlierCount
              << " matches=" << estimate.matchCount << " coverage=" << estimate.coverage
              << " rms=" << estimate.rmsResidualPx << " conf=" << estimate.confidence << "\n";
}

void ExpectAcceptedNear(
    char const* label,
    VisualEstimate const& estimate,
    double expectedTx,
    double expectedTy,
    double expectedScale,
    double expectedRadians,
    double translationTol,
    double scaleTol,
    double degreeTol)
{
    PrintErrors(label, estimate, expectedTx, expectedTy, expectedScale, expectedRadians);
    EXPECT_EQ(estimate.reject, VisualReject::Ok) << VisualRejectName(estimate.reject);
    EXPECT_GT(estimate.confidence, 0.0);
    EXPECT_NEAR(estimate.tx, expectedTx, translationTol);
    EXPECT_NEAR(estimate.ty, expectedTy, translationTol);
    EXPECT_NEAR(estimate.uniformScale, expectedScale, scaleTol);
    EXPECT_NEAR(estimate.radiansClockwise, expectedRadians, degreeTol * kPi / 180.0);
}

} // namespace

TEST(VisualTracker, RejectNameOk)
{
    EXPECT_STREQ(VisualRejectName(VisualReject::Ok), "ok");
    EXPECT_STREQ(VisualRejectName(VisualReject::ReflectionUnsupported), "reflection-unsupported");
}

TEST(VisualTracker, CorrespondenceTranslation)
{
    Transform2D const transform = Translate(Space::C, Space::C, 12.0, -8.0);
    std::vector<Correspondence> const points = GridCorrespondences(transform);
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    ExpectAcceptedNear("corr-translation", estimate, 12.0, -8.0, 1.0, 0.0, 0.05, 0.002, 0.05);
}

TEST(VisualTracker, CorrespondenceUniformScale)
{
    Vec2 const center{kWidth * 0.5, kHeight * 0.5};
    Transform2D const transform = ScaleAbout(Space::C, center, 1.25);
    double tx = 0.0;
    double ty = 0.0;
    double scale = 0.0;
    double radians = 0.0;
    DecomposeExpected(transform, tx, ty, scale, radians);
    std::vector<Correspondence> const points = GridCorrespondences(transform);
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    ExpectAcceptedNear("corr-scale", estimate, tx, ty, scale, radians, 0.1, 0.005, 0.1);
}

TEST(VisualTracker, CorrespondenceOffCenterRotation)
{
    Vec2 const pivot{110.0, 80.0};
    Transform2D const transform = RotateAbout(Space::C, pivot, 18.0 * kPi / 180.0);
    double tx = 0.0;
    double ty = 0.0;
    double scale = 0.0;
    double radians = 0.0;
    DecomposeExpected(transform, tx, ty, scale, radians);
    std::vector<Correspondence> const points = GridCorrespondences(transform);
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    ExpectAcceptedNear("corr-rotate", estimate, tx, ty, scale, radians, 0.15, 0.005, 0.2);
}

TEST(VisualTracker, CorrespondenceOutliersRejectedByRansac)
{
    Transform2D const transform = Translate(Space::C, Space::C, 9.0, 6.0);
    std::vector<Correspondence> points = GridCorrespondences(transform, 28);
    std::size_t const inlierCount = points.size();
    for (int i = 0; i < 8; ++i)
    {
        Correspondence outlier{};
        outlier.priorX = 30.0 + i * 12.0;
        outlier.priorY = 30.0 + i * 8.0;
        outlier.currentX = 280.0 - i * 5.0;
        outlier.currentY = 200.0 + i * 4.0;
        points.push_back(outlier);
    }
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    ExpectAcceptedNear("corr-outliers", estimate, 9.0, 6.0, 1.0, 0.0, 0.2, 0.01, 0.2);
    EXPECT_GE(estimate.inlierCount, static_cast<int>(inlierCount) - 2);
    EXPECT_LT(estimate.inlierCount, static_cast<int>(points.size()));
}

TEST(VisualTracker, CorrespondencePoorCoverageRejected)
{
    std::vector<Correspondence> points;
    for (int y = 8; y <= 36; y += 4)
    {
        for (int x = 8; x <= 36; x += 4)
        {
            Correspondence item{};
            item.priorX = static_cast<double>(x);
            item.priorY = static_cast<double>(y);
            item.currentX = item.priorX + 2.0;
            item.currentY = item.priorY;
            points.push_back(item);
        }
    }
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    EXPECT_EQ(estimate.reject, VisualReject::PoorCoverage) << VisualRejectName(estimate.reject);
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, CorrespondenceImplausibleScaleRejected)
{
    Vec2 const origin{0.0, 0.0};
    Transform2D const transform = ScaleAbout(Space::C, origin, 8.0);
    std::vector<Correspondence> const points = GridCorrespondences(transform, 32);
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    EXPECT_EQ(estimate.reject, VisualReject::ImplausibleMotion) << VisualRejectName(estimate.reject);
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, CorrespondenceShearRejected)
{
    std::vector<Correspondence> points;
    for (int y = 24; y < kHeight - 24; y += 24)
    {
        for (int x = 24; x < kWidth - 24; x += 24)
        {
            Correspondence item{};
            item.priorX = static_cast<double>(x);
            item.priorY = static_cast<double>(y);
            item.currentX = item.priorX + 0.45 * item.priorY;
            item.currentY = item.priorY;
            points.push_back(item);
        }
    }
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    EXPECT_EQ(estimate.reject, VisualReject::ShearOrNonuniformScale)
        << VisualRejectName(estimate.reject) << " shear=" << estimate.shearAmount
        << " aniso=" << estimate.scaleAnisotropy;
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, CorrespondenceHorizontalFlipRejected)
{
    Vec2 const center{kWidth * 0.5, kHeight * 0.5};
    Transform2D const toOrigin = Translate(Space::C, Space::C, -center.x, -center.y);
    Transform2D const flip = Flip(Space::C, Space::C, true, false);
    Transform2D const fromOrigin = Translate(Space::C, Space::C, center.x, center.y);
    Transform2D const transform = MustCompose(MustCompose(toOrigin, flip), fromOrigin);
    std::vector<Correspondence> const points = GridCorrespondences(transform);
    VisualEstimate const estimate =
        EstimateFromCorrespondences(points.data(), points.size(), kWidth, kHeight);
    EXPECT_EQ(estimate.reject, VisualReject::ReflectionUnsupported)
        << VisualRejectName(estimate.reject) << " det=" << estimate.affineDet
        << " scale=" << estimate.uniformScale;
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, BlankSceneRejected)
{
    cv::Mat const blank = cv::Mat::zeros(kHeight, kWidth, CV_8UC1);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(blank), GrayView(blank));
    EXPECT_EQ(estimate.reject, VisualReject::BlankOrLowTexture) << VisualRejectName(estimate.reject);
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, LowTextureGradientRejected)
{
    cv::Mat gradient(kHeight, kWidth, CV_8UC1);
    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            gradient.at<std::uint8_t>(y, x) =
                static_cast<std::uint8_t>(x * 255 / std::max(kWidth - 1, 1));
        }
    }
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(gradient), GrayView(gradient));
    EXPECT_EQ(estimate.reject, VisualReject::BlankOrLowTexture) << VisualRejectName(estimate.reject);
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, TexturedTranslation)
{
    cv::Mat const prior = MakeTexturedGray(0xA11CEu);
    Transform2D const transform = Translate(Space::C, Space::C, 11.0, -7.0);
    cv::Mat const current = WarpGray(prior, transform);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    ExpectAcceptedNear("img-translation", estimate, 11.0, -7.0, 1.0, 0.0, 1.5, 0.03, 0.8);
}

TEST(VisualTracker, TexturedUniformScale)
{
    cv::Mat const prior = MakeTexturedGray(0x5Ca1Eu);
    Vec2 const center{kWidth * 0.5, kHeight * 0.5};
    Transform2D const transform = ScaleAbout(Space::C, center, 1.2);
    double tx = 0.0;
    double ty = 0.0;
    double scale = 0.0;
    double radians = 0.0;
    DecomposeExpected(transform, tx, ty, scale, radians);
    cv::Mat const current = WarpGray(prior, transform);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    ExpectAcceptedNear("img-scale", estimate, tx, ty, scale, radians, 2.0, 0.04, 1.0);
}

TEST(VisualTracker, TexturedOffCenterRotation)
{
    cv::Mat const prior = MakeTexturedGray(0x407u);
    Vec2 const pivot{118.0, 86.0};
    Transform2D const transform = RotateAbout(Space::C, pivot, 12.0 * kPi / 180.0);
    double tx = 0.0;
    double ty = 0.0;
    double scale = 0.0;
    double radians = 0.0;
    DecomposeExpected(transform, tx, ty, scale, radians);
    cv::Mat const current = WarpGray(prior, transform);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    ExpectAcceptedNear("img-rotate", estimate, tx, ty, scale, radians, 2.5, 0.04, 1.2);
}

TEST(VisualTracker, TexturedCombinedPanZoomRotate)
{
    cv::Mat const prior = MakeTexturedGray(0xC0FFu);
    Vec2 const center{kWidth * 0.5, kHeight * 0.5};
    Transform2D const scaled = ScaleAbout(Space::C, center, 1.12);
    Transform2D const rotated = RotateAbout(Space::C, Vec2{140.0, 100.0}, 8.0 * kPi / 180.0);
    Transform2D const translated = Translate(Space::C, Space::C, 7.0, -5.0);
    Transform2D const transform = MustCompose(MustCompose(scaled, rotated), translated);
    double tx = 0.0;
    double ty = 0.0;
    double scale = 0.0;
    double radians = 0.0;
    DecomposeExpected(transform, tx, ty, scale, radians);
    cv::Mat const current = WarpGray(prior, transform);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    ExpectAcceptedNear("img-combined", estimate, tx, ty, scale, radians, 3.0, 0.05, 1.5);
}

TEST(VisualTracker, ChangingStrokeOutliersStillAccepted)
{
    cv::Mat const prior = MakeTexturedGray(0x5700u);
    Transform2D const transform = Translate(Space::C, Space::C, 8.0, 5.0);
    cv::Mat current = WarpGray(prior, transform);
    cv::polylines(
        current,
        std::vector<std::vector<cv::Point>>{{cv::Point(20, 30), cv::Point(80, 90), cv::Point(40, 180),
                                            cv::Point(120, 140), cv::Point(200, 210)}},
        false, cv::Scalar(255), 14, cv::LINE_8);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    ExpectAcceptedNear("img-stroke", estimate, 8.0, 5.0, 1.0, 0.0, 2.0, 0.04, 1.0);
}

TEST(VisualTracker, BgraPackedInputMatchesGray)
{
    cv::Mat const prior = MakeTexturedGray(0xB6AAu);
    Transform2D const transform = Translate(Space::C, Space::C, 6.0, 4.0);
    cv::Mat const current = WarpGray(prior, transform);
    cv::Mat priorBgra;
    cv::Mat currentBgra;
    cv::cvtColor(prior, priorBgra, cv::COLOR_GRAY2BGRA);
    cv::cvtColor(current, currentBgra, cv::COLOR_GRAY2BGRA);
    VisualEstimate const estimate = EstimateCanvasMotion(BgraView(priorBgra), BgraView(currentBgra));
    ExpectAcceptedNear("img-bgra", estimate, 6.0, 4.0, 1.0, 0.0, 1.5, 0.03, 0.8);
}

TEST(VisualTracker, ImageShearRejected)
{
    cv::Mat const prior = MakeTexturedGray(0x5EA4u);
    cv::Mat affine = cv::Mat::zeros(2, 3, CV_64F);
    affine.at<double>(0, 0) = 1.0;
    affine.at<double>(0, 1) = 0.45;
    affine.at<double>(1, 1) = 1.0;
    cv::Mat const current = WarpGrayMat(prior, affine);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    EXPECT_NE(estimate.reject, VisualReject::Ok) << VisualRejectName(estimate.reject);
    EXPECT_EQ(estimate.confidence, 0.0);
    EXPECT_TRUE(estimate.reject == VisualReject::ShearOrNonuniformScale ||
                estimate.reject == VisualReject::HighResidual ||
                estimate.reject == VisualReject::InsufficientInliers)
        << VisualRejectName(estimate.reject);
}

TEST(VisualTracker, ImageHorizontalFlipNotConfidentRotation)
{
    cv::Mat const prior = MakeTexturedGray(0xF11Fu);
    Vec2 const center{kWidth * 0.5, kHeight * 0.5};
    Transform2D const toOrigin = Translate(Space::C, Space::C, -center.x, -center.y);
    Transform2D const flip = Flip(Space::C, Space::C, true, false);
    Transform2D const fromOrigin = Translate(Space::C, Space::C, center.x, center.y);
    Transform2D const transform = MustCompose(MustCompose(toOrigin, flip), fromOrigin);
    cv::Mat const current = WarpGray(prior, transform);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    std::cout << "img-flip reject=" << VisualRejectName(estimate.reject)
              << " conf=" << estimate.confidence << " det=" << estimate.affineDet
              << " scale=" << estimate.uniformScale
              << " deg=" << (estimate.radiansClockwise * 180.0 / kPi) << "\n";
    EXPECT_NE(estimate.reject, VisualReject::Ok) << "partial-affine must not accept a flip";
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, ImageVerticalFlipNotConfidentRotation)
{
    cv::Mat const prior = MakeTexturedGray(0xF11Fu + 3);
    Vec2 const center{kWidth * 0.5, kHeight * 0.5};
    Transform2D const toOrigin = Translate(Space::C, Space::C, -center.x, -center.y);
    Transform2D const flip = Flip(Space::C, Space::C, false, true);
    Transform2D const fromOrigin = Translate(Space::C, Space::C, center.x, center.y);
    Transform2D const transform = MustCompose(MustCompose(toOrigin, flip), fromOrigin);
    cv::Mat const current = WarpGray(prior, transform);
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(prior), GrayView(current));
    std::cout << "img-flip-y reject=" << VisualRejectName(estimate.reject)
              << " conf=" << estimate.confidence << " det=" << estimate.affineDet << "\n";
    EXPECT_NE(estimate.reject, VisualReject::Ok);
    EXPECT_EQ(estimate.confidence, 0.0);
}

TEST(VisualTracker, DegenerateSizeRejected)
{
    cv::Mat tiny(8, 8, CV_8UC1, cv::Scalar(128));
    VisualEstimate const estimate = EstimateCanvasMotion(GrayView(tiny), GrayView(tiny));
    EXPECT_EQ(estimate.reject, VisualReject::DegenerateSize);
    EXPECT_EQ(estimate.confidence, 0.0);
}
