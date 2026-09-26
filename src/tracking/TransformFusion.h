#pragma once

#include "platform/AccessibilityObserver.h"
#include "platform/InputObserver.h"
#include "tracking/NavigatorObserver.h"
#include "tracking/VisualTracker.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace tracing::tracking {

// Timestamped gated fusion of visual, Navigator, optional input, and optional
// accessibility samples in overlay-local similarity components. Mixes
// translation, log-scale, and unwrapped angle within one parity hypothesis.
// Never linearly averages reflection matrices. Independent of rendering.

inline constexpr std::chrono::milliseconds kFusionVisualTtl{300};
inline constexpr std::chrono::milliseconds kFusionNavigatorTtl{300};

enum class FusionSource
{
    None,
    Visual,
    Navigator,
    Input,
    Accessibility,
};

enum class FusionMode
{
    VisualOnly,
    Hybrid,
};

enum class FusionReject
{
    Ok,
    NoUsableSample,
    WrongGeneration,
    Stale,
    DelayedFrame,
    ComponentConflict,
    ParityConflict,
    NonFinite,
};

struct FusionSample
{
    FusionSource source = FusionSource::None;
    bool usable = false;
    bool isAnchor = false;
    bool isPrediction = false;
    std::int64_t timestampMs = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    std::uint64_t calibrationGeneration = 0;
    bool hasTranslation = false;
    bool hasScale = false;
    bool hasRotation = false;
    bool hasParity = false;
    double tx = 0.0;
    double ty = 0.0;
    double uniformScale = 1.0;
    double logScale = 0.0;
    double radiansClockwise = 0.0;
    bool flipX = false;
    bool flipY = false;
    double confidence = 0.0;
};

struct FusionMapping
{
    double documentWidth = 1.0;
    double documentHeight = 1.0;
    double overlayScale = 1.0;
};

struct FusionContext
{
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    std::uint64_t calibrationGeneration = 0;
    std::int64_t nowMs = 0;
};

struct FusionOptions
{
    std::chrono::milliseconds visualTtl = kFusionVisualTtl;
    std::chrono::milliseconds navigatorTtl = kFusionNavigatorTtl;
    std::chrono::milliseconds inputTtl = platform::kMaxPredictionAge;
    double contradictionTranslationPx = 12.0;
    double contradictionLogScale = 0.05;
    double contradictionRadians = 5.0 * 3.14159265358979323846 / 180.0;
};

struct FusionInputs
{
    bool hasVisual = false;
    FusionSample visual{};
    bool hasNavigator = false;
    FusionSample navigator{};
    bool hasInput = false;
    FusionSample input{};
    bool hasAccessibility = false;
    FusionSample accessibility{};
};

struct FusionResult
{
    FusionReject reject = FusionReject::NoUsableSample;
    FusionMode mode = FusionMode::VisualOnly;
    bool accepted = false;
    double tx = 0.0;
    double ty = 0.0;
    double uniformScale = 1.0;
    double logScale = 0.0;
    double radiansClockwise = 0.0;
    bool flipX = false;
    bool flipY = false;
    double confidence = 0.0;
    std::int64_t timestampMs = 0;
    std::int64_t observationAgeMs = 0;
    bool usedVisual = false;
    bool usedNavigator = false;
    bool usedInput = false;
    bool usedAccessibility = false;
    double visualWeight = 0.0;
    double navigatorWeight = 0.0;
    double inputWeight = 0.0;
    double accessibilityWeight = 0.0;
};

struct FusionPose
{
    double tx = 0.0;
    double ty = 0.0;
    double uniformScale = 1.0;
    double logScale = 0.0;
    double radiansClockwise = 0.0;
    bool flipX = false;
    bool flipY = false;
};

char const* FormatFusionSource(FusionSource source) noexcept;
char const* FormatFusionMode(FusionMode mode) noexcept;
char const* FormatFusionReject(FusionReject reject) noexcept;
std::string FormatFusionResult(FusionResult const& result);

FusionSample FromVisual(VisualEstimate const& estimate);
FusionSample FromNavigator(NavigatorObservation const& observation, FusionMapping const& mapping);
FusionSample FromInput(platform::InputObservation const& observation, FusionPose const* lastPose);
FusionSample FromAccessibility(
    platform::AccessibilitySnapshot const& snapshot,
    FusionPose const* lastPose);

class TransformFusion
{
public:
    TransformFusion();
    explicit TransformFusion(FusionOptions options);

    void Reset() noexcept;
    FusionResult Fuse(FusionContext const& context, FusionInputs const& inputs);
    FusionResult const& Last() const noexcept;
    bool HasAcceptedPose() const noexcept;
    FusionPose LastPose() const noexcept;

private:
    FusionOptions options_{};
    FusionResult last_{};
    FusionPose lastPose_{};
    bool hasAccepted_ = false;
    std::int64_t lastVisualTimestampMs_ = 0;
    bool lastFlipX_ = false;
    bool lastFlipY_ = false;
};

} // namespace tracing::tracking
