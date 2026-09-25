#include "tracking/VisualTracker.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tracing::tracking {
namespace {

bool IsFiniteValue(double value) noexcept
{
    return std::isfinite(value);
}

VisualEstimate MakeRejected(VisualReject reason, VisualEstimate estimate = {})
{
    estimate.reject = reason;
    estimate.confidence = 0.0;
    return estimate;
}

bool ViewUsable(CanvasView view, VisualTrackerOptions const& options) noexcept
{
    if (view.data == nullptr || view.width < options.minWidth || view.height < options.minHeight)
    {
        return false;
    }
    int const minStride = view.format == PixelFormat::Bgra32 ? view.width * 4 : view.width;
    return view.stride >= minStride;
}

cv::Mat ToGray(CanvasView view)
{
    int const type = view.format == PixelFormat::Bgra32 ? CV_8UC4 : CV_8UC1;
    cv::Mat wrapped(view.height, view.width, type, const_cast<std::uint8_t*>(view.data),
                    static_cast<std::size_t>(view.stride));
    cv::Mat gray;
    if (view.format == PixelFormat::Bgra32)
    {
        cv::cvtColor(wrapped, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        gray = wrapped.clone();
    }
    return gray;
}

double TextureVariance(cv::Mat const& gray)
{
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_32F, 3);
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    return stddev[0] * stddev[0];
}

void ApplySimilarity(
    double a,
    double b,
    double c,
    double d,
    double tx,
    double ty,
    double x,
    double y,
    double& outX,
    double& outY) noexcept
{
    outX = a * x + b * y + tx;
    outY = c * x + d * y + ty;
}

double CoverageScore(
    std::vector<cv::Point2f> const& prior,
    cv::Mat const& inliers,
    int imageWidth,
    int imageHeight,
    int bins,
    int& occupiedBins)
{
    occupiedBins = 0;
    if (bins < 1 || imageWidth <= 0 || imageHeight <= 0 || prior.empty())
    {
        return 0.0;
    }
    std::vector<char> occupied(static_cast<std::size_t>(bins * bins), 0);
    int const count = static_cast<int>(prior.size());
    for (int i = 0; i < count; ++i)
    {
        if (!inliers.empty() && inliers.at<std::uint8_t>(i) == 0)
        {
            continue;
        }
        int binX = static_cast<int>(prior[static_cast<std::size_t>(i)].x * bins / imageWidth);
        int binY = static_cast<int>(prior[static_cast<std::size_t>(i)].y * bins / imageHeight);
        binX = std::clamp(binX, 0, bins - 1);
        binY = std::clamp(binY, 0, bins - 1);
        occupied[static_cast<std::size_t>(binY * bins + binX)] = 1;
    }
    for (char flag : occupied)
    {
        occupiedBins += flag != 0 ? 1 : 0;
    }
    return static_cast<double>(occupiedBins) / static_cast<double>(bins * bins);
}

double RmsResidual(
    std::vector<cv::Point2f> const& prior,
    std::vector<cv::Point2f> const& current,
    cv::Mat const& inliers,
    cv::Mat const& affine)
{
    if (affine.empty() || prior.size() != current.size() || prior.empty())
    {
        return 0.0;
    }
    double const a = affine.at<double>(0, 0);
    double const b = affine.at<double>(0, 1);
    double const tx = affine.at<double>(0, 2);
    double const c = affine.at<double>(1, 0);
    double const d = affine.at<double>(1, 1);
    double const ty = affine.at<double>(1, 2);
    double sumSq = 0.0;
    int used = 0;
    int const count = static_cast<int>(prior.size());
    for (int i = 0; i < count; ++i)
    {
        if (!inliers.empty() && inliers.at<std::uint8_t>(i) == 0)
        {
            continue;
        }
        double mappedX = 0.0;
        double mappedY = 0.0;
        ApplySimilarity(a, b, c, d, tx, ty, prior[static_cast<std::size_t>(i)].x,
                        prior[static_cast<std::size_t>(i)].y, mappedX, mappedY);
        double const dx = mappedX - current[static_cast<std::size_t>(i)].x;
        double const dy = mappedY - current[static_cast<std::size_t>(i)].y;
        sumSq += dx * dx + dy * dy;
        ++used;
    }
    if (used <= 0)
    {
        return 0.0;
    }
    return std::sqrt(sumSq / static_cast<double>(used));
}

void MeasureLinear(
    cv::Mat const& affine,
    double& det,
    double& shear,
    double& anisotropy)
{
    det = 0.0;
    shear = 0.0;
    anisotropy = 0.0;
    if (affine.empty() || affine.rows < 2 || affine.cols < 2)
    {
        return;
    }
    double const a = affine.at<double>(0, 0);
    double const b = affine.at<double>(0, 1);
    double const c = affine.at<double>(1, 0);
    double const d = affine.at<double>(1, 1);
    det = a * d - b * c;
    double const sx = std::hypot(a, c);
    double const sy = std::hypot(b, d);
    double const maxS = std::max(sx, sy);
    if (maxS > 1e-12)
    {
        anisotropy = std::abs(sx - sy) / maxS;
        double const denom = sx * sy;
        if (denom > 1e-12)
        {
            shear = std::abs(a * b + c * d) / denom;
        }
    }
}

VisualEstimate FinishEstimate(
    VisualEstimate estimate,
    cv::Mat const& partial,
    cv::Mat const& fullAffine,
    std::vector<cv::Point2f> const& prior,
    std::vector<cv::Point2f> const& current,
    cv::Mat const& inliers,
    int imageWidth,
    int imageHeight,
    VisualTrackerOptions const& options)
{
    if (partial.empty())
    {
        return MakeRejected(VisualReject::InsufficientInliers, estimate);
    }
    double const a = partial.at<double>(0, 0);
    double const b = partial.at<double>(0, 1);
    double const tx = partial.at<double>(0, 2);
    double const c = partial.at<double>(1, 0);
    double const d = partial.at<double>(1, 1);
    double const ty = partial.at<double>(1, 2);
    if (!IsFiniteValue(a) || !IsFiniteValue(b) || !IsFiniteValue(c) || !IsFiniteValue(d) ||
        !IsFiniteValue(tx) || !IsFiniteValue(ty))
    {
        return MakeRejected(VisualReject::NonFinite, estimate);
    }

    estimate.tx = tx;
    estimate.ty = ty;
    estimate.uniformScale = std::hypot(a, c);
    estimate.radiansClockwise = std::atan2(c, a);

    int occupiedBins = 0;
    estimate.coverage =
        CoverageScore(prior, inliers, imageWidth, imageHeight, options.coverageBins, occupiedBins);
    estimate.inlierCount = 0;
    if (!inliers.empty())
    {
        estimate.inlierCount = cv::countNonZero(inliers);
    }
    estimate.rmsResidualPx = RmsResidual(prior, current, inliers, partial);

    cv::Mat const linearSource = fullAffine.empty() ? partial : fullAffine;
    MeasureLinear(linearSource, estimate.affineDet, estimate.shearAmount, estimate.scaleAnisotropy);

    if (estimate.affineDet < -options.reflectionDetEpsilon)
    {
        return MakeRejected(VisualReject::ReflectionUnsupported, estimate);
    }
    if (estimate.shearAmount > options.maxShear ||
        estimate.scaleAnisotropy > options.maxScaleAnisotropy)
    {
        return MakeRejected(VisualReject::ShearOrNonuniformScale, estimate);
    }
    if (estimate.inlierCount < options.minInliers)
    {
        return MakeRejected(VisualReject::InsufficientInliers, estimate);
    }
    if (occupiedBins < options.minOccupiedBins)
    {
        return MakeRejected(VisualReject::PoorCoverage, estimate);
    }
    if (estimate.rmsResidualPx > options.maxRmsResidualPx)
    {
        return MakeRejected(VisualReject::HighResidual, estimate);
    }
    if (estimate.uniformScale < options.minScale || estimate.uniformScale > options.maxScale)
    {
        return MakeRejected(VisualReject::ImplausibleMotion, estimate);
    }
    double const maxTranslation =
        options.maxTranslationFactor * static_cast<double>(std::max(imageWidth, imageHeight));
    if (std::hypot(estimate.tx, estimate.ty) > maxTranslation)
    {
        return MakeRejected(VisualReject::ImplausibleMotion, estimate);
    }

    double const matchDenom =
        estimate.matchCount > 0 ? static_cast<double>(estimate.matchCount) : 1.0;
    double const inlierRatio = std::clamp(static_cast<double>(estimate.inlierCount) / matchDenom, 0.0, 1.0);
    double const residualScore = std::clamp(
        1.0 - (estimate.rmsResidualPx / std::max(options.maxRmsResidualPx, 1e-6)), 0.0, 1.0);
    estimate.confidence = inlierRatio * estimate.coverage * residualScore;
    estimate.reject = VisualReject::Ok;
    return estimate;
}

} // namespace

char const* VisualRejectName(VisualReject reject) noexcept
{
    switch (reject)
    {
    case VisualReject::Ok:
        return "ok";
    case VisualReject::DegenerateSize:
        return "degenerate-size";
    case VisualReject::BlankOrLowTexture:
        return "blank-or-low-texture";
    case VisualReject::InsufficientFeatures:
        return "insufficient-features";
    case VisualReject::InsufficientMatches:
        return "insufficient-matches";
    case VisualReject::InsufficientInliers:
        return "insufficient-inliers";
    case VisualReject::PoorCoverage:
        return "poor-coverage";
    case VisualReject::HighResidual:
        return "high-residual";
    case VisualReject::ShearOrNonuniformScale:
        return "shear-or-nonuniform-scale";
    case VisualReject::ImplausibleMotion:
        return "implausible-motion";
    case VisualReject::ReflectionUnsupported:
        return "reflection-unsupported";
    case VisualReject::NonFinite:
        return "non-finite";
    }
    return "unknown";
}

VisualEstimate EstimateFromCorrespondences(
    Correspondence const* points,
    std::size_t count,
    int imageWidth,
    int imageHeight,
    VisualTrackerOptions const& options)
{
    VisualEstimate estimate{};
    estimate.matchCount = static_cast<int>(count);
    if (imageWidth < options.minWidth || imageHeight < options.minHeight)
    {
        return MakeRejected(VisualReject::DegenerateSize, estimate);
    }
    if (points == nullptr || count < static_cast<std::size_t>(options.minMatches))
    {
        return MakeRejected(VisualReject::InsufficientMatches, estimate);
    }

    cv::theRNG().state = options.rngSeed;
    cv::setNumThreads(1);

    std::vector<cv::Point2f> prior;
    std::vector<cv::Point2f> current;
    prior.reserve(count);
    current.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!IsFiniteValue(points[i].priorX) || !IsFiniteValue(points[i].priorY) ||
            !IsFiniteValue(points[i].currentX) || !IsFiniteValue(points[i].currentY))
        {
            return MakeRejected(VisualReject::NonFinite, estimate);
        }
        prior.emplace_back(static_cast<float>(points[i].priorX), static_cast<float>(points[i].priorY));
        current.emplace_back(static_cast<float>(points[i].currentX),
                             static_cast<float>(points[i].currentY));
    }

    cv::Mat inliers;
    cv::Mat const partial = cv::estimateAffinePartial2D(
        prior, current, inliers, cv::RANSAC, options.ransacReprojPx, 2000, 0.99, 10);
    cv::Mat const fullAffine = cv::estimateAffine2D(
        prior, current, cv::noArray(), cv::RANSAC, options.ransacReprojPx, 2000, 0.99, 10);
    return FinishEstimate(
        estimate, partial, fullAffine, prior, current, inliers, imageWidth, imageHeight, options);
}

VisualEstimate EstimateCanvasMotion(
    CanvasView prior,
    CanvasView current,
    VisualTrackerOptions const& options)
{
    VisualEstimate estimate{};
    if (!ViewUsable(prior, options) || !ViewUsable(current, options) ||
        prior.width != current.width || prior.height != current.height)
    {
        return MakeRejected(VisualReject::DegenerateSize, estimate);
    }

    cv::theRNG().state = options.rngSeed;
    cv::setNumThreads(1);

    cv::Mat const priorGray = ToGray(prior);
    cv::Mat const currentGray = ToGray(current);
    if (TextureVariance(priorGray) < options.minTextureVariance ||
        TextureVariance(currentGray) < options.minTextureVariance)
    {
        return MakeRejected(VisualReject::BlankOrLowTexture, estimate);
    }

    cv::Ptr<cv::ORB> orb = cv::ORB::create(options.orbFeatures, 1.2f, 8, 15, 0, 2,
                                           cv::ORB::HARRIS_SCORE, 31, 20);
    std::vector<cv::KeyPoint> priorKeys;
    std::vector<cv::KeyPoint> currentKeys;
    cv::Mat priorDesc;
    cv::Mat currentDesc;
    orb->detectAndCompute(priorGray, cv::noArray(), priorKeys, priorDesc);
    orb->detectAndCompute(currentGray, cv::noArray(), currentKeys, currentDesc);
    estimate.featureCount = static_cast<int>(std::min(priorKeys.size(), currentKeys.size()));
    if (priorDesc.empty() || currentDesc.empty() || estimate.featureCount < options.minMatches)
    {
        return MakeRejected(VisualReject::InsufficientFeatures, estimate);
    }

    cv::BFMatcher matcher(cv::NORM_HAMMING, false);
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(priorDesc, currentDesc, knn, 2);
    std::vector<Correspondence> matches;
    matches.reserve(knn.size());
    for (std::vector<cv::DMatch> const& pair : knn)
    {
        if (pair.size() < 2)
        {
            continue;
        }
        if (pair[0].distance > options.matchRatio * pair[1].distance)
        {
            continue;
        }
        cv::KeyPoint const& from = priorKeys[static_cast<std::size_t>(pair[0].queryIdx)];
        cv::KeyPoint const& to = currentKeys[static_cast<std::size_t>(pair[0].trainIdx)];
        Correspondence item{};
        item.priorX = from.pt.x;
        item.priorY = from.pt.y;
        item.currentX = to.pt.x;
        item.currentY = to.pt.y;
        matches.push_back(item);
    }
    estimate.matchCount = static_cast<int>(matches.size());
    if (estimate.matchCount < options.minMatches)
    {
        return MakeRejected(VisualReject::InsufficientMatches, estimate);
    }

    VisualEstimate fitted = EstimateFromCorrespondences(
        matches.data(), matches.size(), prior.width, prior.height, options);
    fitted.featureCount = estimate.featureCount;
    fitted.matchCount = estimate.matchCount;
    return fitted;
}

} // namespace tracing::tracking
