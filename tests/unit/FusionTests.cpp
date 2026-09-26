#include <gtest/gtest.h>

#include "tracking/TransformFusion.h"

#include <cmath>
#include <string>
#include <tuple>

namespace {

using tracing::platform::AccessibilityCapability;
using tracing::platform::AccessibilityReject;
using tracing::platform::AccessibilitySnapshot;
using tracing::platform::InputObservation;
using tracing::platform::InputReject;
using tracing::platform::InputSource;
using tracing::tracking::FormatFusionMode;
using tracing::tracking::FormatFusionReject;
using tracing::tracking::FormatFusionResult;
using tracing::tracking::FromAccessibility;
using tracing::tracking::FromInput;
using tracing::tracking::FromNavigator;
using tracing::tracking::FromVisual;
using tracing::tracking::FusionContext;
using tracing::tracking::FusionInputs;
using tracing::tracking::FusionMapping;
using tracing::tracking::FusionMode;
using tracing::tracking::FusionPose;
using tracing::tracking::FusionReject;
using tracing::tracking::FusionResult;
using tracing::tracking::FusionSample;
using tracing::tracking::NavigatorObservation;
using tracing::tracking::NavigatorReject;
using tracing::tracking::NavigatorSource;
using tracing::tracking::TransformFusion;
using tracing::tracking::VisualEstimate;
using tracing::tracking::VisualReject;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

FusionContext Ctx(std::int64_t nowMs)
{
    FusionContext context{};
    context.targetGeneration = 1;
    context.geometryGeneration = 2;
    context.calibrationGeneration = 3;
    context.nowMs = nowMs;
    return context;
}

void Stamp(FusionSample& sample, FusionContext const& context, std::int64_t timestampMs)
{
    sample.targetGeneration = context.targetGeneration;
    sample.geometryGeneration = context.geometryGeneration;
    sample.calibrationGeneration = context.calibrationGeneration;
    sample.timestampMs = timestampMs;
}

VisualEstimate OkVisual(
    double tx,
    double ty,
    double scale = 1.0,
    double radians = 0.0,
    bool flipX = false,
    bool flipY = false,
    double confidence = 0.8)
{
    VisualEstimate estimate{};
    estimate.reject = VisualReject::Ok;
    estimate.tx = tx;
    estimate.ty = ty;
    estimate.uniformScale = scale;
    estimate.radiansClockwise = radians;
    estimate.flipX = flipX;
    estimate.flipY = flipY;
    estimate.confidence = confidence;
    estimate.inlierCount = 20;
    return estimate;
}

FusionSample VisualSample(
    FusionContext const& context,
    std::int64_t timestampMs,
    double tx,
    double ty,
    double scale = 1.0,
    double radians = 0.0,
    bool flipX = false,
    bool flipY = false,
    double confidence = 0.8)
{
    FusionSample sample = FromVisual(OkVisual(tx, ty, scale, radians, flipX, flipY, confidence));
    Stamp(sample, context, timestampMs);
    return sample;
}

FusionSample NavigatorDropout()
{
    NavigatorObservation observation{};
    observation.source = NavigatorSource::Missing;
    observation.reject = NavigatorReject::BlankOrNoIndicator;
    return FromNavigator(observation, FusionMapping{});
}

FusionSample UnsupportedUia(FusionContext const& context)
{
    AccessibilitySnapshot snapshot{};
    snapshot.capability = AccessibilityCapability::Unsupported;
    snapshot.reject = AccessibilityReject::Unsupported;
    snapshot.enabled = false;
    snapshot.targetGeneration = context.targetGeneration;
    FusionSample sample = FromAccessibility(snapshot, nullptr);
    Stamp(sample, context, context.nowMs);
    return sample;
}

InputObservation PredictedPan(double dx, double dy, std::int64_t ticksMs, std::uint64_t generation)
{
    InputObservation observation{};
    observation.source = InputSource::Predicted;
    observation.reject = InputReject::Ok;
    observation.ticksMs = ticksMs;
    observation.targetGeneration = generation;
    observation.hasTranslation = true;
    observation.dx = dx;
    observation.dy = dy;
    observation.confidence = 0.25;
    observation.pending = true;
    observation.ageMs = 0;
    return observation;
}

FusionInputs VisualOnly(FusionSample visual)
{
    FusionInputs inputs{};
    inputs.hasVisual = true;
    inputs.visual = visual;
    return inputs;
}

double Wrap(double radians)
{
    constexpr double kTwoPi = 2.0 * kPi;
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

} // namespace

TEST(Fusion, VisualOnlyIdentityPass)
{
    TransformFusion fusion;
    FusionContext const context = Ctx(0);
    FusionSample const visual = VisualSample(context, 0, 8.0, -5.0, 1.25, 0.1);
    FusionResult const result = fusion.Fuse(context, VisualOnly(visual));

    ASSERT_TRUE(result.accepted) << FormatFusionResult(result);
    EXPECT_EQ(result.reject, FusionReject::Ok);
    EXPECT_EQ(result.mode, FusionMode::VisualOnly);
    EXPECT_TRUE(result.usedVisual);
    EXPECT_FALSE(result.usedNavigator);
    EXPECT_FALSE(result.usedInput);
    EXPECT_FALSE(result.usedAccessibility);
    EXPECT_NEAR(result.tx, 8.0, 1.0e-12);
    EXPECT_NEAR(result.ty, -5.0, 1.0e-12);
    EXPECT_NEAR(result.uniformScale, 1.25, 1.0e-12);
    EXPECT_NEAR(result.radiansClockwise, 0.1, 1.0e-12);
    EXPECT_NE(FormatFusionResult(result).find("visual-only"), std::string::npos);
    EXPECT_STREQ(FormatFusionMode(result.mode), "visual-only");
}

TEST(Fusion, ObserverDropoutKeepsVisual)
{
    TransformFusion fusion;
    FusionContext const context = Ctx(20);
    FusionInputs inputs = VisualOnly(VisualSample(context, 20, 3.0, 4.0));
    inputs.hasNavigator = true;
    inputs.navigator = NavigatorDropout();
    Stamp(inputs.navigator, context, 20);
    inputs.hasAccessibility = true;
    inputs.accessibility = UnsupportedUia(context);

    FusionResult const result = fusion.Fuse(context, inputs);
    ASSERT_TRUE(result.accepted) << FormatFusionResult(result);
    EXPECT_EQ(result.mode, FusionMode::VisualOnly);
    EXPECT_TRUE(result.usedVisual);
    EXPECT_FALSE(result.usedNavigator);
    EXPECT_FALSE(result.usedAccessibility);
    EXPECT_NEAR(result.tx, 3.0, 1.0e-12);
    EXPECT_NEAR(result.ty, 4.0, 1.0e-12);
}

TEST(Fusion, DelayedVisualDoesNotOverwriteNewer)
{
    TransformFusion fusion;
    FusionContext const firstCtx = Ctx(80);
    FusionResult const first =
        fusion.Fuse(firstCtx, VisualOnly(VisualSample(firstCtx, 80, 10.0, 0.0)));
    ASSERT_TRUE(first.accepted);
    EXPECT_NEAR(first.tx, 10.0, 1.0e-12);

    FusionContext const lateCtx = Ctx(90);
    FusionResult const delayed =
        fusion.Fuse(lateCtx, VisualOnly(VisualSample(lateCtx, 10, 99.0, 50.0)));
    EXPECT_FALSE(delayed.accepted);
    EXPECT_EQ(delayed.reject, FusionReject::DelayedFrame);
    EXPECT_STREQ(FormatFusionReject(delayed.reject), "delayed-frame");
    EXPECT_NEAR(fusion.LastPose().tx, 10.0, 1.0e-12);
    EXPECT_NEAR(fusion.LastPose().ty, 0.0, 1.0e-12);
}

TEST(Fusion, AngleWraparoundIsSmallStep)
{
    TransformFusion fusion;
    FusionContext const firstCtx = Ctx(0);
    FusionResult const first =
        fusion.Fuse(firstCtx, VisualOnly(VisualSample(firstCtx, 0, 0.0, 0.0, 1.0, 179.0 * kDeg)));
    ASSERT_TRUE(first.accepted);

    FusionContext const secondCtx = Ctx(16);
    FusionResult const second =
        fusion.Fuse(secondCtx, VisualOnly(VisualSample(secondCtx, 16, 0.0, 0.0, 1.0, -179.0 * kDeg)));
    ASSERT_TRUE(second.accepted) << FormatFusionResult(second);
    double const step = Wrap(second.radiansClockwise - first.radiansClockwise);
    EXPECT_NEAR(step, 2.0 * kDeg, 1.0e-9);
    EXPECT_LT(std::fabs(step), 10.0 * kDeg);
}

TEST(Fusion, ContradictoryParityRejectsWithoutAveraging)
{
    TransformFusion fusion;
    FusionContext const context = Ctx(0);
    FusionInputs inputs = VisualOnly(VisualSample(context, 0, 1.0, 0.0, 1.0, 0.0, true, false));
    FusionSample navigator = VisualSample(context, 0, 1.0, 0.0, 1.0, 0.0, false, true, 0.7);
    navigator.source = tracing::tracking::FusionSource::Navigator;
    navigator.isAnchor = true;
    navigator.isPrediction = false;
    inputs.hasNavigator = true;
    inputs.navigator = navigator;

    FusionResult const result = fusion.Fuse(context, inputs);
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reject, FusionReject::ParityConflict);
    EXPECT_DOUBLE_EQ(result.confidence, 0.0);
    EXPECT_FALSE(fusion.HasAcceptedPose());
    EXPECT_NE(FormatFusionResult(result).find("parity-conflict"), std::string::npos);
}

TEST(Fusion, InputCorrectionPrefersVisualAnchor)
{
    TransformFusion fusion;
    FusionContext const context = Ctx(40);
    FusionPose last{};
    FusionSample input = FromInput(PredictedPan(20.0, 0.0, 40, context.targetGeneration), &last);
    Stamp(input, context, 40);

    FusionInputs inputs = VisualOnly(VisualSample(context, 40, 0.0, 0.0));
    inputs.hasInput = true;
    inputs.input = input;

    FusionResult const result = fusion.Fuse(context, inputs);
    ASSERT_TRUE(result.accepted) << FormatFusionResult(result);
    EXPECT_TRUE(result.usedVisual);
    EXPECT_FALSE(result.usedInput);
    EXPECT_NEAR(result.tx, 0.0, 1.0e-12);
    EXPECT_NEAR(result.ty, 0.0, 1.0e-12);
}

TEST(Fusion, RelocalizationReplacesWrongPrediction)
{
    TransformFusion fusion;
    FusionContext const predictCtx = Ctx(0);
    FusionPose identity{};
    FusionSample input =
        FromInput(PredictedPan(40.0, 8.0, 0, predictCtx.targetGeneration), &identity);
    Stamp(input, predictCtx, 0);
    FusionInputs predicted{};
    predicted.hasInput = true;
    predicted.input = input;
    FusionResult const gap = fusion.Fuse(predictCtx, predicted);
    ASSERT_TRUE(gap.accepted) << FormatFusionResult(gap);
    EXPECT_EQ(gap.mode, FusionMode::Hybrid);
    EXPECT_TRUE(gap.usedInput);
    EXPECT_NEAR(gap.tx, 40.0, 1.0e-12);

    FusionContext const visualCtx = Ctx(40);
    FusionResult const relocated =
        fusion.Fuse(visualCtx, VisualOnly(VisualSample(visualCtx, 40, 2.0, 1.0, 1.0, 0.0, false, false, 0.9)));
    ASSERT_TRUE(relocated.accepted) << FormatFusionResult(relocated);
    EXPECT_TRUE(relocated.usedVisual);
    EXPECT_FALSE(relocated.usedInput);
    EXPECT_NEAR(relocated.tx, 2.0, 1.0e-12);
    EXPECT_NEAR(relocated.ty, 1.0, 1.0e-12);
}

TEST(Fusion, SameReplayHybridImprovesLatencyWithoutConfidentWrong)
{
    auto visualOnlyReplay = [](bool contradictingVisual) {
        TransformFusion fusion;
        int acceptedEarly = 0;
        int confidentWrong = 0;
        FusionContext const t0 = Ctx(0);
        FusionResult const early = fusion.Fuse(t0, FusionInputs{});
        if (early.accepted && early.confidence >= 0.5 && std::fabs(early.tx - 10.0) > 12.0)
        {
            ++confidentWrong;
        }
        if (early.accepted)
        {
            ++acceptedEarly;
        }

        FusionContext const t80 = Ctx(80);
        double const truth = contradictingVisual ? 0.0 : 10.0;
        FusionResult const late =
            fusion.Fuse(t80, VisualOnly(VisualSample(t80, 80, truth, 0.0)));
        if (late.accepted && late.confidence >= 0.5 && std::fabs(late.tx - truth) > 12.0)
        {
            ++confidentWrong;
        }
        return std::tuple<int, int, bool, double>(
            acceptedEarly, confidentWrong, late.accepted, late.accepted ? late.tx : 0.0);
    };

    auto hybridReplay = [](bool contradictingVisual) {
        TransformFusion fusion;
        int acceptedEarly = 0;
        int confidentWrong = 0;
        FusionContext const t0 = Ctx(0);
        FusionPose identity{};
        FusionSample input = FromInput(PredictedPan(10.0, 0.0, 0, t0.targetGeneration), &identity);
        Stamp(input, t0, 0);
        FusionInputs predicted{};
        predicted.hasInput = true;
        predicted.input = input;
        FusionResult const early = fusion.Fuse(t0, predicted);
        if (early.accepted)
        {
            ++acceptedEarly;
        }
        if (early.accepted && early.confidence >= 0.5 && std::fabs(early.tx - 10.0) > 12.0)
        {
            ++confidentWrong;
        }

        FusionContext const t80 = Ctx(80);
        double const truth = contradictingVisual ? 0.0 : 10.0;
        FusionSample lateInput =
            FromInput(PredictedPan(10.0, 0.0, 0, t80.targetGeneration), &identity);
        Stamp(lateInput, t80, 0);
        if (t80.nowMs - lateInput.timestampMs > tracing::platform::kMaxPredictionAge.count())
        {
            lateInput.usable = false;
        }
        FusionInputs lateInputs = VisualOnly(VisualSample(t80, 80, truth, 0.0));
        lateInputs.hasInput = true;
        lateInputs.input = lateInput;
        FusionResult const late = fusion.Fuse(t80, lateInputs);
        if (late.accepted && late.confidence >= 0.5 && std::fabs(late.tx - truth) > 12.0)
        {
            ++confidentWrong;
        }
        return std::tuple<int, int, bool, double>(
            acceptedEarly, confidentWrong, late.accepted, late.accepted ? late.tx : 0.0);
    };

    auto const visualAgree = visualOnlyReplay(false);
    auto const hybridAgree = hybridReplay(false);
    EXPECT_EQ(std::get<0>(visualAgree), 0);
    EXPECT_EQ(std::get<0>(hybridAgree), 1);
    EXPECT_TRUE(std::get<2>(visualAgree));
    EXPECT_TRUE(std::get<2>(hybridAgree));
    EXPECT_NEAR(std::get<3>(visualAgree), 10.0, 1.0e-9);
    EXPECT_NEAR(std::get<3>(hybridAgree), 10.0, 1.0e-9);
    EXPECT_LE(std::get<1>(hybridAgree), std::get<1>(visualAgree));

    auto const visualContra = visualOnlyReplay(true);
    auto const hybridContra = hybridReplay(true);
    EXPECT_TRUE(std::get<2>(visualContra));
    EXPECT_TRUE(std::get<2>(hybridContra));
    EXPECT_NEAR(std::get<3>(visualContra), 0.0, 1.0e-9);
    EXPECT_NEAR(std::get<3>(hybridContra), 0.0, 1.0e-9);
    EXPECT_EQ(std::get<1>(hybridContra), 0);
    EXPECT_LE(std::get<1>(hybridContra), std::get<1>(visualContra));
}

TEST(Fusion, WrongGenerationAndStaleAreRejected)
{
    TransformFusion fusion;
    FusionContext context = Ctx(0);
    FusionSample visual = VisualSample(context, 0, 1.0, 2.0);
    visual.targetGeneration = 9;
    FusionResult const wrong = fusion.Fuse(context, VisualOnly(visual));
    EXPECT_FALSE(wrong.accepted);
    EXPECT_EQ(wrong.reject, FusionReject::WrongGeneration);

    FusionContext staleCtx = Ctx(400);
    FusionResult const stale =
        fusion.Fuse(staleCtx, VisualOnly(VisualSample(staleCtx, 0, 1.0, 2.0)));
    EXPECT_FALSE(stale.accepted);
    EXPECT_EQ(stale.reject, FusionReject::Stale);
}

TEST(Fusion, NavigatorUvMapsWithoutInventingZoomPercent)
{
    NavigatorObservation observation{};
    observation.source = NavigatorSource::Observed;
    observation.reject = NavigatorReject::Ok;
    observation.hasTranslation = true;
    observation.hasRelativeScale = true;
    observation.centerU = 0.25;
    observation.centerV = 0.50;
    observation.sizeU = 0.20;
    observation.sizeV = 0.20;
    observation.confidence = 0.6;
    observation.hasZoomPercent = false;
    FusionMapping mapping{};
    mapping.documentWidth = 400.0;
    mapping.documentHeight = 200.0;
    mapping.overlayScale = 1.0;

    FusionSample const sample = FromNavigator(observation, mapping);
    ASSERT_TRUE(sample.usable);
    EXPECT_TRUE(sample.hasTranslation);
    EXPECT_FALSE(sample.hasScale);
    EXPECT_NEAR(sample.tx, 100.0, 1.0e-12);
    EXPECT_NEAR(sample.ty, 100.0, 1.0e-12);

    AccessibilitySnapshot snapshot{};
    snapshot.capability = AccessibilityCapability::Unsupported;
    snapshot.enabled = false;
    FusionSample const uia = FromAccessibility(snapshot, nullptr);
    EXPECT_FALSE(uia.usable);
}

TEST(Fusion, StaleInputDoesNotContribute)
{
    TransformFusion fusion;
    FusionContext const context = Ctx(120);
    FusionPose identity{};
    FusionSample input = FromInput(PredictedPan(15.0, 0.0, 0, context.targetGeneration), &identity);
    Stamp(input, context, 0);
    FusionInputs inputs{};
    inputs.hasInput = true;
    inputs.input = input;
    FusionResult const result = fusion.Fuse(context, inputs);
    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reject, FusionReject::Stale);
    EXPECT_FALSE(result.usedInput);
}
