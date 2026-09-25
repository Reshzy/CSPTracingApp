#pragma once

#include <cstddef>
#include <cstdint>

namespace tracing::tracking {

// Canvas-to-canvas visual estimator. Compares prior/keyframe CSP pixels with
// current CSP pixels. Does not accept an imported tracing reference. Reflection
// is rejected, not recovered, in this step.

enum class PixelFormat
{
    Gray8,
    Bgra32,
};

struct CanvasView
{
    std::uint8_t const* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::Gray8;
};

struct Correspondence
{
    double priorX = 0.0;
    double priorY = 0.0;
    double currentX = 0.0;
    double currentY = 0.0;
};

enum class VisualReject
{
    Ok,
    DegenerateSize,
    BlankOrLowTexture,
    InsufficientFeatures,
    InsufficientMatches,
    InsufficientInliers,
    PoorCoverage,
    HighResidual,
    ShearOrNonuniformScale,
    ImplausibleMotion,
    ReflectionUnsupported,
    NonFinite,
};

struct VisualTrackerOptions
{
    int minWidth = 16;
    int minHeight = 16;
    double minTextureVariance = 8.0;
    int orbFeatures = 800;
    float matchRatio = 0.75f;
    double ransacReprojPx = 3.0;
    int minMatches = 12;
    int minInliers = 10;
    int coverageBins = 3;
    int minOccupiedBins = 3;
    double maxRmsResidualPx = 2.5;
    double minScale = 0.25;
    double maxScale = 4.0;
    double maxTranslationFactor = 1.5;
    double maxShear = 0.12;
    double maxScaleAnisotropy = 0.12;
    double reflectionDetEpsilon = 1e-6;
    std::uint64_t rngSeed = 1;
};

struct VisualEstimate
{
    VisualReject reject = VisualReject::DegenerateSize;
    double tx = 0.0;
    double ty = 0.0;
    double uniformScale = 1.0;
    double radiansClockwise = 0.0;
    int featureCount = 0;
    int matchCount = 0;
    int inlierCount = 0;
    double coverage = 0.0;
    double rmsResidualPx = 0.0;
    double shearAmount = 0.0;
    double scaleAnisotropy = 0.0;
    double affineDet = 0.0;
    double confidence = 0.0;
};

char const* VisualRejectName(VisualReject reject) noexcept;

VisualEstimate EstimateFromCorrespondences(
    Correspondence const* points,
    std::size_t count,
    int imageWidth,
    int imageHeight,
    VisualTrackerOptions const& options = {});

VisualEstimate EstimateCanvasMotion(
    CanvasView prior,
    CanvasView current,
    VisualTrackerOptions const& options = {});

} // namespace tracing::tracking
