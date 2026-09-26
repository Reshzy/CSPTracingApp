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

constexpr double kPi = 3.14159265358979323846;

bool IsFiniteValue(double value) noexcept
{
    return std::isfinite(value);
}

VisualEstimate MakeRejected(VisualReject reason, VisualEstimate estimate = {})
{
    estimate.reject = reason;
    estimate.confidence = 0.0;
    estimate.flipX = false;
    estimate.flipY = false;
    estimate.parity = VisualParity::None;
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

void RemapPriorPoint(double& x, double& y, VisualParity parity, int imageWidth, int imageHeight) noexcept
{
    if (parity == VisualParity::FlipX)
    {
        x = static_cast<double>(imageWidth - 1) - x;
    }
    else if (parity == VisualParity::FlipY)
    {
        y = static_cast<double>(imageHeight - 1) - y;
    }
}

double WrapAngle(double radians) noexcept
{
    while (radians > kPi)
    {
        radians -= 2.0 * kPi;
    }
    while (radians < -kPi)
    {
        radians += 2.0 * kPi;
    }
    return radians;
}

void CanonicalizeAngleParity(VisualEstimate& estimate) noexcept
{
    if (!estimate.flipX && !estimate.flipY)
    {
        return;
    }
    if (std::fabs(WrapAngle(estimate.radiansClockwise)) <= (kPi * 0.5))
    {
        return;
    }
    estimate.radiansClockwise = WrapAngle(estimate.radiansClockwise - kPi);
    bool const wasX = estimate.flipX;
    estimate.flipX = estimate.flipY;
    estimate.flipY = wasX;
    estimate.parity = VisualParityFromFlags(estimate.flipX, estimate.flipY);
}

void BakeParity(
    VisualEstimate& estimate,
    double a,
    double b,
    double c,
    double d,
    double tx,
    double ty,
    VisualParity parity,
    int imageWidth,
    int imageHeight) noexcept
{
    estimate.parity = VisualParity::None;
    estimate.flipX = false;
    estimate.flipY = false;
    if (parity == VisualParity::FlipX)
    {
        estimate.tx = a * static_cast<double>(imageWidth - 1) + tx;
        estimate.ty = c * static_cast<double>(imageWidth - 1) + ty;
        estimate.flipX = true;
        estimate.parity = VisualParity::FlipX;
        estimate.affineDet = (-a) * d - b * (-c);
    }
    else if (parity == VisualParity::FlipY)
    {
        estimate.tx = b * static_cast<double>(imageHeight - 1) + tx;
        estimate.ty = d * static_cast<double>(imageHeight - 1) + ty;
        estimate.flipY = true;
        estimate.parity = VisualParity::FlipY;
        estimate.affineDet = a * (-d) - (-b) * c;
    }
    CanonicalizeAngleParity(estimate);
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
    VisualParity parity,
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

    BakeParity(estimate, a, b, c, d, tx, ty, parity, imageWidth, imageHeight);
    if (!IsFiniteValue(estimate.tx) || !IsFiniteValue(estimate.ty) ||
        !IsFiniteValue(estimate.affineDet))
    {
        return MakeRejected(VisualReject::NonFinite, estimate);
    }
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

VisualEstimate FitOrientationPreserving(
    Correspondence const* points,
    std::size_t count,
    int imageWidth,
    int imageHeight,
    VisualParity remap,
    VisualParity bake,
    VisualTrackerOptions const& options)
{
    VisualEstimate estimate{};
    estimate.matchCount = static_cast<int>(count);
    estimate.parity = bake;
    if (imageWidth < options.minWidth || imageHeight < options.minHeight)
    {
        return MakeRejected(VisualReject::DegenerateSize, estimate);
    }
    if (points == nullptr || count < static_cast<std::size_t>(options.minMatches))
    {
        return MakeRejected(VisualReject::InsufficientMatches, estimate);
    }

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
        double priorX = points[i].priorX;
        double priorY = points[i].priorY;
        RemapPriorPoint(priorX, priorY, remap, imageWidth, imageHeight);
        prior.emplace_back(static_cast<float>(priorX), static_cast<float>(priorY));
        current.emplace_back(static_cast<float>(points[i].currentX),
                             static_cast<float>(points[i].currentY));
    }

    cv::Mat inliers;
    cv::Mat const partial = cv::estimateAffinePartial2D(
        prior, current, inliers, cv::RANSAC, options.ransacReprojPx, 2000, 0.99, 10);
    cv::Mat const fullAffine = cv::estimateAffine2D(
        prior, current, cv::noArray(), cv::RANSAC, options.ransacReprojPx, 2000, 0.99, 10);
    return FinishEstimate(
        estimate,
        partial,
        fullAffine,
        prior,
        current,
        inliers,
        imageWidth,
        imageHeight,
        bake,
        options);
}

void MapEstimate(VisualEstimate const& estimate, double x, double y, double& outX, double& outY) noexcept
{
    double const px = estimate.flipX ? -x : x;
    double const py = estimate.flipY ? -y : y;
    double const cosine = std::cos(estimate.radiansClockwise);
    double const sine = std::sin(estimate.radiansClockwise);
    double const qx = estimate.uniformScale * px;
    double const qy = estimate.uniformScale * py;
    outX = cosine * qx + (-sine) * qy + estimate.tx;
    outY = sine * qx + cosine * qy + estimate.ty;
}

bool EstimatesMapClose(
    VisualEstimate const& left,
    VisualEstimate const& right,
    int imageWidth,
    int imageHeight,
    double tolerance) noexcept
{
    double const samples[3][2] = {
        {0.0, 0.0},
        {static_cast<double>(imageWidth) * 0.5, 0.0},
        {0.0, static_cast<double>(imageHeight) * 0.5},
    };
    for (auto const& sample : samples)
    {
        double lx = 0.0;
        double ly = 0.0;
        double rx = 0.0;
        double ry = 0.0;
        MapEstimate(left, sample[0], sample[1], lx, ly);
        MapEstimate(right, sample[0], sample[1], rx, ry);
        if (std::hypot(lx - rx, ly - ry) > tolerance)
        {
            return false;
        }
    }
    return true;
}

VisualEstimate SelectParity(
    VisualEstimate none,
    VisualEstimate flipX,
    VisualEstimate flipY,
    int imageWidth,
    int imageHeight,
    VisualTrackerOptions const& options)
{
    if (flipX.reject == VisualReject::Ok && flipY.reject == VisualReject::Ok)
    {
        bool const sameParity = flipX.parity == flipY.parity;
        bool const equivalent180 =
            std::fabs(std::fabs(WrapAngle(flipX.radiansClockwise - flipY.radiansClockwise)) - kPi) <
            (20.0 * kPi / 180.0);
        bool const sameMap = EstimatesMapClose(flipX, flipY, imageWidth, imageHeight, 2.5);
        if (sameParity || equivalent180 || sameMap)
        {
            if (flipX.confidence >= flipY.confidence)
            {
                flipY = MakeRejected(VisualReject::ReflectionUnsupported, flipY);
            }
            else
            {
                flipX = MakeRejected(VisualReject::ReflectionUnsupported, flipX);
            }
        }
    }
    if (none.reject == VisualReject::Ok && flipX.reject == VisualReject::Ok &&
        EstimatesMapClose(none, flipX, imageWidth, imageHeight, 2.5))
    {
        flipX = MakeRejected(VisualReject::ReflectionUnsupported, flipX);
    }
    if (none.reject == VisualReject::Ok && flipY.reject == VisualReject::Ok &&
        EstimatesMapClose(none, flipY, imageWidth, imageHeight, 2.5))
    {
        flipY = MakeRejected(VisualReject::ReflectionUnsupported, flipY);
    }

    auto dropWorseResidual = [](VisualEstimate& a, VisualEstimate& b) {
        if (a.reject != VisualReject::Ok || b.reject != VisualReject::Ok)
        {
            return;
        }
        if (a.rmsResidualPx + 0.12 < b.rmsResidualPx)
        {
            b = MakeRejected(VisualReject::HighResidual, b);
        }
        else if (b.rmsResidualPx + 0.12 < a.rmsResidualPx)
        {
            a = MakeRejected(VisualReject::HighResidual, a);
        }
    };
    dropWorseResidual(none, flipX);
    dropWorseResidual(none, flipY);

    bool const noneNear180 =
        none.reject == VisualReject::Ok &&
        std::fabs(std::fabs(WrapAngle(none.radiansClockwise)) - kPi) < (20.0 * kPi / 180.0);
    if (noneNear180)
    {
        auto dropIfNotBetter = [&](VisualEstimate& flip) {
            if (flip.reject != VisualReject::Ok)
            {
                return;
            }
            if (flip.rmsResidualPx + 0.05 >= none.rmsResidualPx)
            {
                flip = MakeRejected(VisualReject::ReflectionUnsupported, flip);
            }
        };
        dropIfNotBetter(flipX);
        dropIfNotBetter(flipY);
    }

    VisualEstimate const* candidates[3] = {&none, &flipX, &flipY};
    VisualParity const parities[3] = {
        VisualParity::None, VisualParity::FlipX, VisualParity::FlipY};

    int bestOk = -1;
    int secondOk = -1;
    int preferredOk = -1;
    int okCount = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (candidates[i]->reject != VisualReject::Ok)
        {
            continue;
        }
        ++okCount;
        if (parities[i] == options.preferredParity)
        {
            preferredOk = i;
        }
        if (bestOk < 0 || candidates[i]->confidence > candidates[bestOk]->confidence)
        {
            secondOk = bestOk;
            bestOk = i;
        }
        else if (secondOk < 0 || candidates[i]->confidence > candidates[secondOk]->confidence)
        {
            secondOk = i;
        }
    }

    if (bestOk < 0)
    {
        if (none.reject != VisualReject::DegenerateSize)
        {
            return none;
        }
        if (flipX.inlierCount >= flipY.inlierCount)
        {
            return flipX;
        }
        return flipY;
    }

    if (okCount >= 2 && secondOk >= 0 && parities[bestOk] != parities[secondOk])
    {
        double const gap =
            candidates[bestOk]->confidence - candidates[secondOk]->confidence;
        if (gap < options.parityMargin)
        {
            return MakeRejected(VisualReject::ParityAmbiguous, *candidates[bestOk]);
        }
    }

    if (preferredOk >= 0)
    {
        double const gap =
            candidates[bestOk]->confidence - candidates[preferredOk]->confidence;
        if (gap <= options.parityMargin)
        {
            return *candidates[preferredOk];
        }
    }

    return *candidates[bestOk];
}

int OrbFlipCode(VisualParity parity) noexcept
{
    if (parity == VisualParity::FlipX)
    {
        return 1;
    }
    if (parity == VisualParity::FlipY)
    {
        return 0;
    }
    return -2;
}

VisualEstimate MatchAndFit(
    cv::Mat const& priorGray,
    std::vector<cv::KeyPoint> const& currentKeys,
    cv::Mat const& currentDesc,
    VisualParity parity,
    VisualTrackerOptions const& options)
{
    VisualEstimate estimate{};
    estimate.parity = parity;
    cv::Mat priorForHyp = priorGray;
    int const flipCode = OrbFlipCode(parity);
    if (flipCode >= 0)
    {
        cv::flip(priorGray, priorForHyp, flipCode);
    }

    cv::Ptr<cv::ORB> orb = cv::ORB::create(options.orbFeatures, 1.2f, 8, 15, 0, 2,
                                           cv::ORB::HARRIS_SCORE, 31, 20);
    std::vector<cv::KeyPoint> priorKeys;
    cv::Mat priorDesc;
    orb->detectAndCompute(priorForHyp, cv::noArray(), priorKeys, priorDesc);
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

    // Keypoints are already in remapped prior coordinates.
    VisualEstimate fitted = FitOrientationPreserving(
        matches.data(),
        matches.size(),
        priorGray.cols,
        priorGray.rows,
        VisualParity::None,
        parity,
        options);
    fitted.featureCount = estimate.featureCount;
    fitted.matchCount = estimate.matchCount;
    return fitted;
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
    case VisualReject::ParityAmbiguous:
        return "parity-ambiguous";
    case VisualReject::NonFinite:
        return "non-finite";
    }
    return "unknown";
}

char const* VisualParityName(VisualParity parity) noexcept
{
    switch (parity)
    {
    case VisualParity::None:
        return "none";
    case VisualParity::FlipX:
        return "x";
    case VisualParity::FlipY:
        return "y";
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
    cv::theRNG().state = options.rngSeed;
    cv::setNumThreads(1);

    VisualEstimate const none = FitOrientationPreserving(
        points, count, imageWidth, imageHeight, VisualParity::None, VisualParity::None, options);
    VisualEstimate const flipX = FitOrientationPreserving(
        points, count, imageWidth, imageHeight, VisualParity::FlipX, VisualParity::FlipX, options);
    VisualEstimate const flipY = FitOrientationPreserving(
        points, count, imageWidth, imageHeight, VisualParity::FlipY, VisualParity::FlipY, options);
    return SelectParity(none, flipX, flipY, imageWidth, imageHeight, options);
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
    std::vector<cv::KeyPoint> currentKeys;
    cv::Mat currentDesc;
    orb->detectAndCompute(currentGray, cv::noArray(), currentKeys, currentDesc);
    if (currentDesc.empty() || static_cast<int>(currentKeys.size()) < options.minMatches)
    {
        return MakeRejected(VisualReject::InsufficientFeatures, estimate);
    }

    VisualEstimate const none =
        MatchAndFit(priorGray, currentKeys, currentDesc, VisualParity::None, options);
    VisualEstimate const flipX =
        MatchAndFit(priorGray, currentKeys, currentDesc, VisualParity::FlipX, options);
    VisualEstimate const flipY =
        MatchAndFit(priorGray, currentKeys, currentDesc, VisualParity::FlipY, options);
    return SelectParity(none, flipX, flipY, prior.width, prior.height, options);
}

} // namespace tracing::tracking
