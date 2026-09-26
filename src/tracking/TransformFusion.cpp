#include "tracking/TransformFusion.h"

#include <cmath>
#include <cstdio>
#include <optional>

namespace tracing::tracking {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kMinWeight = 1.0e-9;

bool IsFiniteValue(double value) noexcept
{
    return std::isfinite(value);
}

double WrapRadians(double radians) noexcept
{
    while (radians > kPi)
    {
        radians -= kTwoPi;
    }
    while (radians < -kPi)
    {
        radians += kTwoPi;
    }
    return radians;
}

double UnwrapRadians(double radians, double reference) noexcept
{
    return reference + WrapRadians(radians - reference);
}

double SampleWeight(FusionSample const& sample) noexcept
{
    if (!sample.usable)
    {
        return 0.0;
    }
    if (sample.confidence > 0.0 && IsFiniteValue(sample.confidence))
    {
        return sample.confidence;
    }
    return 1.0;
}

bool SampleFinite(FusionSample const& sample) noexcept
{
    if (sample.hasTranslation && (!IsFiniteValue(sample.tx) || !IsFiniteValue(sample.ty)))
    {
        return false;
    }
    if (sample.hasScale &&
        (!IsFiniteValue(sample.logScale) || !IsFiniteValue(sample.uniformScale) ||
         sample.uniformScale <= 0.0))
    {
        return false;
    }
    if (sample.hasRotation && !IsFiniteValue(sample.radiansClockwise))
    {
        return false;
    }
    if (sample.hasParity && (!IsFiniteValue(sample.tx) || !IsFiniteValue(sample.ty)))
    {
        return false;
    }
    return IsFiniteValue(sample.confidence);
}

bool GenerationsMatch(FusionSample const& sample, FusionContext const& context) noexcept
{
    return sample.targetGeneration == context.targetGeneration &&
           sample.geometryGeneration == context.geometryGeneration &&
           sample.calibrationGeneration == context.calibrationGeneration;
}

std::chrono::milliseconds TtlFor(FusionSource source, FusionOptions const& options) noexcept
{
    switch (source)
    {
    case FusionSource::Visual:
        return options.visualTtl;
    case FusionSource::Navigator:
        return options.navigatorTtl;
    case FusionSource::Input:
        return options.inputTtl;
    case FusionSource::Accessibility:
        return options.visualTtl;
    default:
        return options.visualTtl;
    }
}

enum class FilterReason
{
    Ok,
    Absent,
    WrongGeneration,
    Stale,
    DelayedFrame,
    NonFinite,
};

FilterReason FilterSample(
    FusionSample& sample,
    bool present,
    FusionContext const& context,
    FusionOptions const& options,
    bool hasAcceptedVisual,
    std::int64_t lastVisualTimestampMs) noexcept
{
    if (!present || !sample.usable)
    {
        sample.usable = false;
        return FilterReason::Absent;
    }
    if (!SampleFinite(sample))
    {
        sample.usable = false;
        return FilterReason::NonFinite;
    }
    if (!GenerationsMatch(sample, context))
    {
        sample.usable = false;
        return FilterReason::WrongGeneration;
    }
    std::int64_t const ageMs = context.nowMs - sample.timestampMs;
    if (ageMs < 0)
    {
        sample.usable = false;
        return FilterReason::DelayedFrame;
    }
    if (ageMs > TtlFor(sample.source, options).count())
    {
        sample.usable = false;
        return FilterReason::Stale;
    }
    if (sample.source == FusionSource::Visual && hasAcceptedVisual &&
        sample.timestampMs < lastVisualTimestampMs)
    {
        sample.usable = false;
        return FilterReason::DelayedFrame;
    }
    return FilterReason::Ok;
}

FusionReject RejectFromFilter(FilterReason reason) noexcept
{
    switch (reason)
    {
    case FilterReason::WrongGeneration:
        return FusionReject::WrongGeneration;
    case FilterReason::Stale:
        return FusionReject::Stale;
    case FilterReason::DelayedFrame:
        return FusionReject::DelayedFrame;
    case FilterReason::NonFinite:
        return FusionReject::NonFinite;
    default:
        return FusionReject::NoUsableSample;
    }
}

bool TranslationContradicts(
    FusionSample const& left,
    FusionSample const& right,
    FusionOptions const& options) noexcept
{
    if (!left.hasTranslation || !right.hasTranslation)
    {
        return false;
    }
    return std::hypot(left.tx - right.tx, left.ty - right.ty) > options.contradictionTranslationPx;
}

bool ScaleContradicts(
    FusionSample const& left,
    FusionSample const& right,
    FusionOptions const& options) noexcept
{
    if (!left.hasScale || !right.hasScale)
    {
        return false;
    }
    return std::fabs(left.logScale - right.logScale) > options.contradictionLogScale;
}

bool RotationContradicts(
    FusionSample const& left,
    FusionSample const& right,
    FusionOptions const& options) noexcept
{
    if (!left.hasRotation || !right.hasRotation)
    {
        return false;
    }
    return std::fabs(WrapRadians(left.radiansClockwise - right.radiansClockwise)) >
           options.contradictionRadians;
}

bool ParityFlagsEqual(FusionSample const& left, FusionSample const& right) noexcept
{
    return left.flipX == right.flipX && left.flipY == right.flipY;
}

bool PreferLeftAnchor(FusionSample const& left, FusionSample const& right) noexcept
{
    if (left.source == FusionSource::Visual && right.source != FusionSource::Visual)
    {
        return true;
    }
    if (right.source == FusionSource::Visual && left.source != FusionSource::Visual)
    {
        return false;
    }
    return SampleWeight(left) >= SampleWeight(right);
}

void GateConflictingPair(
    FusionSample& left,
    FusionSample& right,
    bool (*contradicts)(FusionSample const&, FusionSample const&, FusionOptions const&),
    FusionOptions const& options,
    bool& conflictReject) noexcept
{
    if (!left.usable || !right.usable)
    {
        return;
    }
    if (!contradicts(left, right, options))
    {
        return;
    }
    bool const leftAnchor = left.isAnchor && !left.isPrediction;
    bool const rightAnchor = right.isAnchor && !right.isPrediction;
    if (leftAnchor && rightAnchor && left.source != FusionSource::Visual &&
        right.source != FusionSource::Visual)
    {
        conflictReject = true;
        left.usable = false;
        right.usable = false;
        return;
    }
    if (PreferLeftAnchor(left, right))
    {
        right.usable = false;
    }
    else
    {
        left.usable = false;
    }
}

void DropInputWhenAnchorPresent(
    FusionSample& input,
    FusionSample const* anchors,
    std::size_t count) noexcept
{
    if (!input.usable || input.source != FusionSource::Input)
    {
        return;
    }
    bool anchorTranslation = false;
    bool anchorScale = false;
    bool anchorRotation = false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!anchors[i].usable || anchors[i].source == FusionSource::Input)
        {
            continue;
        }
        anchorTranslation = anchorTranslation || anchors[i].hasTranslation;
        anchorScale = anchorScale || anchors[i].hasScale;
        anchorRotation = anchorRotation || anchors[i].hasRotation;
    }
    if (anchorTranslation)
    {
        input.hasTranslation = false;
    }
    if (anchorScale)
    {
        input.hasScale = false;
    }
    if (anchorRotation)
    {
        input.hasRotation = false;
    }
    if (!input.hasTranslation && !input.hasScale && !input.hasRotation)
    {
        input.usable = false;
    }
}

struct MixAccum
{
    double txNum = 0.0;
    double txDen = 0.0;
    double tyNum = 0.0;
    double tyDen = 0.0;
    double logNum = 0.0;
    double logDen = 0.0;
    double angNum = 0.0;
    double angDen = 0.0;
    double confNum = 0.0;
    double confDen = 0.0;
    std::optional<double> angleReference;
    std::optional<double> singleUniformScale;
    int translationCount = 0;
    int scaleCount = 0;
    int rotationCount = 0;
    FusionSample const* singleTranslation = nullptr;
    FusionSample const* singleScale = nullptr;
    FusionSample const* singleRotation = nullptr;
};

void AddSample(MixAccum& mix, FusionSample const& sample, double& sourceWeight) noexcept
{
    if (!sample.usable)
    {
        return;
    }
    double const weight = SampleWeight(sample);
    if (weight <= kMinWeight)
    {
        return;
    }
    sourceWeight = weight;
    mix.confNum += weight * sample.confidence;
    mix.confDen += weight;
    if (sample.hasTranslation)
    {
        mix.txNum += weight * sample.tx;
        mix.txDen += weight;
        mix.tyNum += weight * sample.ty;
        mix.tyDen += weight;
        mix.translationCount += 1;
        mix.singleTranslation = &sample;
    }
    if (sample.hasScale)
    {
        mix.logNum += weight * sample.logScale;
        mix.logDen += weight;
        mix.scaleCount += 1;
        mix.singleScale = &sample;
        mix.singleUniformScale = sample.uniformScale;
    }
    if (sample.hasRotation)
    {
        if (!mix.angleReference.has_value())
        {
            mix.angleReference = sample.radiansClockwise;
        }
        double const unwrapped = UnwrapRadians(sample.radiansClockwise, *mix.angleReference);
        mix.angNum += weight * unwrapped;
        mix.angDen += weight;
        mix.rotationCount += 1;
        mix.singleRotation = &sample;
    }
}

char const* FusionParityName(bool flipX, bool flipY) noexcept
{
    if (flipX && !flipY)
    {
        return "flipX";
    }
    if (!flipX && flipY)
    {
        return "flipY";
    }
    return "none";
}

} // namespace

char const* FormatFusionSource(FusionSource source) noexcept
{
    switch (source)
    {
    case FusionSource::None:
        return "none";
    case FusionSource::Visual:
        return "visual";
    case FusionSource::Navigator:
        return "navigator";
    case FusionSource::Input:
        return "input";
    case FusionSource::Accessibility:
        return "uia";
    }
    return "unknown";
}

char const* FormatFusionMode(FusionMode mode) noexcept
{
    switch (mode)
    {
    case FusionMode::VisualOnly:
        return "visual-only";
    case FusionMode::Hybrid:
        return "hybrid";
    }
    return "unknown";
}

char const* FormatFusionReject(FusionReject reject) noexcept
{
    switch (reject)
    {
    case FusionReject::Ok:
        return "ok";
    case FusionReject::NoUsableSample:
        return "no-usable-sample";
    case FusionReject::WrongGeneration:
        return "wrong-generation";
    case FusionReject::Stale:
        return "stale";
    case FusionReject::DelayedFrame:
        return "delayed-frame";
    case FusionReject::ComponentConflict:
        return "component-conflict";
    case FusionReject::ParityConflict:
        return "parity-conflict";
    case FusionReject::NonFinite:
        return "non-finite";
    }
    return "unknown";
}

std::string FormatFusionResult(FusionResult const& result)
{
    std::string text = "fusion mode=";
    text += FormatFusionMode(result.mode);
    text += " reject=";
    text += FormatFusionReject(result.reject);
    text += " accepted=";
    text += result.accepted ? "yes" : "no";
    text += " src=";
    text += result.usedVisual ? "V" : "-";
    text += result.usedNavigator ? "N" : "-";
    text += result.usedInput ? "I" : "-";
    text += result.usedAccessibility ? "U" : "-";
    char number[64]{};
    text += " conf=";
    std::snprintf(number, sizeof(number), "%.3f", result.confidence);
    text += number;
    text += " tx=";
    std::snprintf(number, sizeof(number), "%.3f", result.tx);
    text += number;
    text += " ty=";
    std::snprintf(number, sizeof(number), "%.3f", result.ty);
    text += number;
    text += " scale=";
    std::snprintf(number, sizeof(number), "%.5f", result.uniformScale);
    text += number;
    text += " rad=";
    std::snprintf(number, sizeof(number), "%.5f", result.radiansClockwise);
    text += number;
    text += " parity=";
    text += FusionParityName(result.flipX, result.flipY);
    text += " ageMs=";
    text += std::to_string(result.observationAgeMs);
    return text;
}

FusionSample FromVisual(VisualEstimate const& estimate)
{
    FusionSample sample{};
    sample.source = FusionSource::Visual;
    sample.isAnchor = true;
    sample.isPrediction = false;
    if (estimate.reject != VisualReject::Ok)
    {
        return sample;
    }
    if (!IsFiniteValue(estimate.tx) || !IsFiniteValue(estimate.ty) ||
        !IsFiniteValue(estimate.uniformScale) || estimate.uniformScale <= 0.0 ||
        !IsFiniteValue(estimate.radiansClockwise))
    {
        return sample;
    }
    sample.usable = true;
    sample.hasTranslation = true;
    sample.hasScale = true;
    sample.hasRotation = true;
    sample.hasParity = true;
    sample.tx = estimate.tx;
    sample.ty = estimate.ty;
    sample.uniformScale = estimate.uniformScale;
    sample.logScale = std::log(estimate.uniformScale);
    sample.radiansClockwise = estimate.radiansClockwise;
    sample.flipX = estimate.flipX;
    sample.flipY = estimate.flipY;
    sample.confidence = estimate.confidence > 0.0 ? estimate.confidence : 1.0;
    return sample;
}

FusionSample FromNavigator(NavigatorObservation const& observation, FusionMapping const& mapping)
{
    FusionSample sample{};
    sample.source = FusionSource::Navigator;
    sample.isAnchor = true;
    sample.isPrediction = false;
    sample.timestampMs = observation.captureTicks;
    sample.targetGeneration = observation.targetGeneration;
    sample.geometryGeneration = observation.geometryGeneration;
    if (observation.source != NavigatorSource::Observed || observation.reject != NavigatorReject::Ok)
    {
        return sample;
    }
    if (!IsFiniteValue(observation.confidence))
    {
        return sample;
    }

    double const width = mapping.documentWidth > 0.0 ? mapping.documentWidth : 1.0;
    double const height = mapping.documentHeight > 0.0 ? mapping.documentHeight : 1.0;
    double const overlay = mapping.overlayScale > 0.0 ? mapping.overlayScale : 1.0;

    if (observation.hasTranslation)
    {
        double tx = 0.0;
        double ty = 0.0;
        if (observation.hasDocumentPosition)
        {
            tx = observation.documentX;
            ty = observation.documentY;
        }
        else
        {
            tx = observation.centerU * width;
            ty = observation.centerV * height;
        }
        tx *= overlay;
        ty *= overlay;
        if (!IsFiniteValue(tx) || !IsFiniteValue(ty))
        {
            return sample;
        }
        sample.hasTranslation = true;
        sample.tx = tx;
        sample.ty = ty;
    }
    if (observation.hasRotation && IsFiniteValue(observation.radiansClockwise))
    {
        sample.hasRotation = true;
        sample.radiansClockwise = observation.radiansClockwise;
    }
    if (observation.hasZoomPercent && observation.zoomPercent > 0.0 &&
        IsFiniteValue(observation.zoomPercent))
    {
        sample.hasScale = true;
        sample.uniformScale = observation.zoomPercent / 100.0;
        sample.logScale = std::log(sample.uniformScale);
    }
    sample.hasParity = observation.hasParity;
    sample.flipX = false;
    sample.flipY = false;
    sample.usable = sample.hasTranslation || sample.hasScale || sample.hasRotation;
    sample.confidence = observation.confidence > 0.0 ? observation.confidence : 1.0;
    return sample;
}

FusionSample FromInput(platform::InputObservation const& observation, FusionPose const* lastPose)
{
    FusionSample sample{};
    sample.source = FusionSource::Input;
    sample.timestampMs = observation.ticksMs;
    sample.targetGeneration = observation.targetGeneration;
    bool const predicted = observation.source == platform::InputSource::Predicted &&
                           observation.pending && observation.reject == platform::InputReject::Ok;
    bool const confirmed = observation.source == platform::InputSource::Confirmed &&
                           observation.reject == platform::InputReject::Ok;
    if (!predicted && !confirmed)
    {
        return sample;
    }
    if (!observation.hasTranslation && !observation.hasScale && !observation.hasRotation)
    {
        return sample;
    }

    FusionPose base{};
    if (lastPose != nullptr)
    {
        base = *lastPose;
    }
    sample.usable = true;
    sample.isPrediction = predicted;
    sample.isAnchor = confirmed;
    sample.confidence = observation.confidence > 0.0 ? observation.confidence : 0.25;
    if (observation.hasTranslation)
    {
        if (!IsFiniteValue(observation.dx) || !IsFiniteValue(observation.dy))
        {
            sample.usable = false;
            return sample;
        }
        sample.hasTranslation = true;
        sample.tx = base.tx + observation.dx;
        sample.ty = base.ty + observation.dy;
    }
    if (observation.hasScale)
    {
        if (!IsFiniteValue(observation.logScale))
        {
            sample.usable = false;
            return sample;
        }
        sample.hasScale = true;
        sample.logScale = base.logScale + observation.logScale;
        sample.uniformScale = std::exp(sample.logScale);
    }
    if (observation.hasRotation)
    {
        if (!IsFiniteValue(observation.radiansClockwise))
        {
            sample.usable = false;
            return sample;
        }
        sample.hasRotation = true;
        sample.radiansClockwise = base.radiansClockwise + observation.radiansClockwise;
    }
    sample.hasParity = false;
    sample.flipX = base.flipX;
    sample.flipY = base.flipY;
    return sample;
}

FusionSample FromAccessibility(
    platform::AccessibilitySnapshot const& snapshot,
    FusionPose const* lastPose)
{
    FusionSample sample{};
    sample.source = FusionSource::Accessibility;
    sample.isAnchor = true;
    sample.isPrediction = false;
    sample.targetGeneration = snapshot.targetGeneration;
    if (snapshot.capability != platform::AccessibilityCapability::Supported || !snapshot.enabled)
    {
        return sample;
    }

    FusionPose base{};
    if (lastPose != nullptr)
    {
        base = *lastPose;
    }
    sample.tx = base.tx;
    sample.ty = base.ty;
    sample.uniformScale = base.uniformScale;
    sample.logScale = base.logScale;
    sample.radiansClockwise = base.radiansClockwise;
    sample.flipX = base.flipX;
    sample.flipY = base.flipY;

    for (auto const& property : snapshot.observed)
    {
        if (!property.hasNumeric || !IsFiniteValue(property.numeric))
        {
            continue;
        }
        if (property.component == platform::AccessibilityComponent::Zoom)
        {
            double scale = property.numeric;
            if (scale > 1.0)
            {
                scale = scale / 100.0;
            }
            if (scale <= 0.0)
            {
                continue;
            }
            sample.hasScale = true;
            sample.uniformScale = scale;
            sample.logScale = std::log(scale);
        }
        else if (property.component == platform::AccessibilityComponent::Rotation)
        {
            sample.hasRotation = true;
            sample.radiansClockwise = property.numeric * kPi / 180.0;
        }
    }
    sample.usable = sample.hasScale || sample.hasRotation;
    sample.confidence = sample.usable ? 0.4 : 0.0;
    return sample;
}

TransformFusion::TransformFusion() = default;

TransformFusion::TransformFusion(FusionOptions options) : options_(options)
{
}

void TransformFusion::Reset() noexcept
{
    last_ = {};
    lastPose_ = {};
    hasAccepted_ = false;
    lastVisualTimestampMs_ = 0;
    lastFlipX_ = false;
    lastFlipY_ = false;
}

FusionResult const& TransformFusion::Last() const noexcept
{
    return last_;
}

bool TransformFusion::HasAcceptedPose() const noexcept
{
    return hasAccepted_;
}

FusionPose TransformFusion::LastPose() const noexcept
{
    return lastPose_;
}

FusionResult TransformFusion::Fuse(FusionContext const& context, FusionInputs const& inputs)
{
    FusionSample visual = inputs.visual;
    FusionSample navigator = inputs.navigator;
    FusionSample input = inputs.input;
    FusionSample accessibility = inputs.accessibility;

    FilterReason const visualFilter = FilterSample(
        visual,
        inputs.hasVisual,
        context,
        options_,
        hasAccepted_,
        lastVisualTimestampMs_);
    FilterReason const navFilter = FilterSample(
        navigator,
        inputs.hasNavigator,
        context,
        options_,
        false,
        0);
    FilterReason const inputFilter = FilterSample(
        input,
        inputs.hasInput,
        context,
        options_,
        false,
        0);
    FilterReason const uiaFilter = FilterSample(
        accessibility,
        inputs.hasAccessibility,
        context,
        options_,
        false,
        0);

    FusionResult result{};
    result.mode = FusionMode::VisualOnly;

    bool const anyPresent = inputs.hasVisual || inputs.hasNavigator || inputs.hasInput ||
                            inputs.hasAccessibility;
    if (!anyPresent)
    {
        result.reject = FusionReject::NoUsableSample;
        last_ = result;
        return result;
    }

    bool const anyUsable = visual.usable || navigator.usable || input.usable || accessibility.usable;
    if (!anyUsable)
    {
        FilterReason reason = visualFilter;
        if (reason == FilterReason::Absent)
        {
            reason = navFilter;
        }
        if (reason == FilterReason::Absent)
        {
            reason = inputFilter;
        }
        if (reason == FilterReason::Absent)
        {
            reason = uiaFilter;
        }
        result.reject = RejectFromFilter(reason);
        last_ = result;
        return result;
    }

    FusionSample* parityHolders[4]{};
    int parityCount = 0;
    FusionSample* all[4] = {&visual, &navigator, &input, &accessibility};
    for (FusionSample* sample : all)
    {
        if (sample->usable && sample->hasParity)
        {
            parityHolders[parityCount++] = sample;
        }
    }
    for (int i = 0; i < parityCount; ++i)
    {
        for (int j = i + 1; j < parityCount; ++j)
        {
            if (!ParityFlagsEqual(*parityHolders[i], *parityHolders[j]))
            {
                result.reject = FusionReject::ParityConflict;
                result.confidence = 0.0;
                result.accepted = false;
                last_ = result;
                return result;
            }
        }
    }

    bool conflictReject = false;
    GateConflictingPair(visual, navigator, TranslationContradicts, options_, conflictReject);
    GateConflictingPair(visual, navigator, ScaleContradicts, options_, conflictReject);
    GateConflictingPair(visual, navigator, RotationContradicts, options_, conflictReject);
    GateConflictingPair(visual, accessibility, TranslationContradicts, options_, conflictReject);
    GateConflictingPair(visual, accessibility, ScaleContradicts, options_, conflictReject);
    GateConflictingPair(visual, accessibility, RotationContradicts, options_, conflictReject);
    GateConflictingPair(navigator, accessibility, TranslationContradicts, options_, conflictReject);
    GateConflictingPair(navigator, accessibility, ScaleContradicts, options_, conflictReject);
    GateConflictingPair(navigator, accessibility, RotationContradicts, options_, conflictReject);
    if (conflictReject)
    {
        result.reject = FusionReject::ComponentConflict;
        result.confidence = 0.0;
        last_ = result;
        return result;
    }

    FusionSample anchors[3] = {visual, navigator, accessibility};
    DropInputWhenAnchorPresent(input, anchors, 3);

    MixAccum mix{};
    AddSample(mix, visual, result.visualWeight);
    AddSample(mix, navigator, result.navigatorWeight);
    AddSample(mix, accessibility, result.accessibilityWeight);
    AddSample(mix, input, result.inputWeight);

    result.usedVisual = visual.usable && result.visualWeight > 0.0;
    result.usedNavigator = navigator.usable && result.navigatorWeight > 0.0;
    result.usedInput = input.usable && result.inputWeight > 0.0;
    result.usedAccessibility = accessibility.usable && result.accessibilityWeight > 0.0;

    if (mix.translationCount == 0 && mix.scaleCount == 0 && mix.rotationCount == 0)
    {
        result.reject = FusionReject::NoUsableSample;
        last_ = result;
        return result;
    }

    if (hasAccepted_)
    {
        result.tx = lastPose_.tx;
        result.ty = lastPose_.ty;
        result.uniformScale = lastPose_.uniformScale;
        result.logScale = lastPose_.logScale;
        result.radiansClockwise = lastPose_.radiansClockwise;
        result.flipX = lastFlipX_;
        result.flipY = lastFlipY_;
    }

    if (mix.txDen > kMinWeight)
    {
        if (mix.translationCount == 1 && mix.singleTranslation != nullptr)
        {
            result.tx = mix.singleTranslation->tx;
            result.ty = mix.singleTranslation->ty;
        }
        else
        {
            result.tx = mix.txNum / mix.txDen;
            result.ty = mix.tyNum / mix.tyDen;
        }
    }
    if (mix.logDen > kMinWeight)
    {
        if (mix.scaleCount == 1 && mix.singleScale != nullptr)
        {
            result.logScale = mix.singleScale->logScale;
            result.uniformScale = mix.singleScale->uniformScale;
        }
        else
        {
            result.logScale = mix.logNum / mix.logDen;
            result.uniformScale = std::exp(result.logScale);
        }
    }
    if (mix.angDen > kMinWeight)
    {
        if (mix.rotationCount == 1 && mix.singleRotation != nullptr)
        {
            result.radiansClockwise = WrapRadians(mix.singleRotation->radiansClockwise);
        }
        else
        {
            result.radiansClockwise = WrapRadians(mix.angNum / mix.angDen);
        }
    }

    if (visual.usable && visual.hasParity)
    {
        result.flipX = visual.flipX;
        result.flipY = visual.flipY;
    }
    else if (navigator.usable && navigator.hasParity)
    {
        result.flipX = navigator.flipX;
        result.flipY = navigator.flipY;
    }
    else if (hasAccepted_)
    {
        result.flipX = lastFlipX_;
        result.flipY = lastFlipY_;
    }

    if (mix.confDen > kMinWeight)
    {
        result.confidence = mix.confNum / mix.confDen;
    }

    result.usedVisual = visual.usable && result.visualWeight > 0.0;
    result.usedNavigator = navigator.usable && result.navigatorWeight > 0.0;
    result.usedInput = input.usable && result.inputWeight > 0.0;
    result.usedAccessibility = accessibility.usable && result.accessibilityWeight > 0.0;
    result.mode = (result.usedNavigator || result.usedInput || result.usedAccessibility)
                      ? FusionMode::Hybrid
                      : FusionMode::VisualOnly;

    std::int64_t latest = 0;
    bool haveTime = false;
    auto considerTime = [&](FusionSample const& sample, bool used) {
        if (!used)
        {
            return;
        }
        if (!haveTime || sample.timestampMs > latest)
        {
            latest = sample.timestampMs;
            haveTime = true;
        }
    };
    considerTime(visual, result.usedVisual);
    considerTime(navigator, result.usedNavigator);
    considerTime(input, result.usedInput);
    considerTime(accessibility, result.usedAccessibility);
    result.timestampMs = haveTime ? latest : context.nowMs;
    result.observationAgeMs = context.nowMs - result.timestampMs;
    if (result.observationAgeMs < 0)
    {
        result.observationAgeMs = 0;
    }

    result.reject = FusionReject::Ok;
    result.accepted = true;

    lastPose_.tx = result.tx;
    lastPose_.ty = result.ty;
    lastPose_.uniformScale = result.uniformScale;
    lastPose_.logScale = result.logScale;
    lastPose_.radiansClockwise = result.radiansClockwise;
    lastPose_.flipX = result.flipX;
    lastPose_.flipY = result.flipY;
    lastFlipX_ = result.flipX;
    lastFlipY_ = result.flipY;
    hasAccepted_ = true;
    if (result.usedVisual)
    {
        lastVisualTimestampMs_ = visual.timestampMs;
    }
    last_ = result;
    return result;
}

} // namespace tracing::tracking
