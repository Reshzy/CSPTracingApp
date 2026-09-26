#pragma once

#include "core/Calibration.h"

#include <cstdint>
#include <string>

namespace tracing::tracking {

// User-calibrated Navigator thumbnail observer. Reports only components that
// image geometry actually measures. Does not invent CSP zoom percent, document
// coordinates, or parity from an uncalibrated or rectangular indicator.
// Independent of TrackingSession / overlay. No OCR.

enum class NavigatorPixelFormat
{
    Gray8,
    Bgra32,
};

enum class NavigatorSource
{
    Disabled,
    UncalibratedRoi,
    Missing,
    Occluded,
    Ambiguous,
    Observed,
};

enum class NavigatorReject
{
    Ok,
    Disabled,
    NoRoi,
    DegenerateSize,
    BlankOrNoIndicator,
    Occluded,
    Ambiguous,
    NonFinite,
};

struct NavigatorView
{
    std::uint8_t const* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    NavigatorPixelFormat format = NavigatorPixelFormat::Gray8;
    std::int64_t captureTicks = 0;
    std::uint64_t sequence = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
};

struct NavigatorMapping
{
    bool applied = false;
    double documentWidth = 0.0;
    double documentHeight = 0.0;
    core::DocumentScaleLabel units = core::DocumentScaleLabel::LocalCalibratedUnits;
};

struct NavigatorObservation
{
    NavigatorSource source = NavigatorSource::Disabled;
    NavigatorReject reject = NavigatorReject::Disabled;
    std::int64_t captureTicks = 0;
    std::uint64_t sequence = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    bool hasTranslation = false;
    bool hasRelativeScale = false;
    bool hasRotation = false;
    bool hasParity = false;
    double centerU = 0.0;
    double centerV = 0.0;
    double sizeU = 0.0;
    double sizeV = 0.0;
    double radiansClockwise = 0.0;
    double confidence = 0.0;
    bool hasDocumentPosition = false;
    bool hasZoomPercent = false;
    double documentX = 0.0;
    double documentY = 0.0;
    double zoomPercent = 0.0;
    core::Rect2 roi{};
    NavigatorMapping mapping{};
    int candidateCount = 0;
};

struct NavigatorObserverOptions
{
    int minWidth = 16;
    int minHeight = 16;
    double minTextureVariance = 4.0;
    double minAreaFraction = 0.02;
    double maxAreaFraction = 0.85;
    double minRectangularity = 0.72;
    double ambiguityRatio = 0.75;
    double edgeMarginPx = 2.0;
    double minAspect = 0.12;
    double maxAspect = 8.0;
    double squareAspectLimit = 0.18;
    double cannyLow = 40.0;
    double cannyHigh = 120.0;
};

char const* FormatNavigatorSource(NavigatorSource source) noexcept;
char const* FormatNavigatorReject(NavigatorReject reject) noexcept;
std::string FormatNavigatorObservation(NavigatorObservation const& observation);

class NavigatorObserver
{
public:
    explicit NavigatorObserver(NavigatorObserverOptions options = {});

    void Disable() noexcept;
    bool SetRoi(
        core::Rect2 roi,
        double clientWidth,
        double clientHeight,
        core::RoiRejectReason& reason);
    void ClearMapping() noexcept;
    void SetMapping(double documentWidth, double documentHeight) noexcept;

    NavigatorObservation Observe(NavigatorView const& view);
    NavigatorObservation const& Last() const noexcept;
    bool Enabled() const noexcept;
    bool RoiApplied() const noexcept;
    NavigatorMapping const& Mapping() const noexcept;
    core::Rect2 Roi() const noexcept;

private:
    NavigatorObservation MakeBase(NavigatorSource source, NavigatorReject reject) const;
    NavigatorObservation ObservePixels(NavigatorView const& view);

    NavigatorObserverOptions options_{};
    bool enabled_ = false;
    bool roiApplied_ = false;
    core::Rect2 roi_{};
    NavigatorMapping mapping_{};
    NavigatorObservation last_{};
};

} // namespace tracing::tracking
