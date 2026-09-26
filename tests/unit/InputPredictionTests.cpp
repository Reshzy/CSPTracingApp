#include <gtest/gtest.h>

#include "platform/InputObserver.h"

#include <chrono>
#include <cmath>
#include <string>

namespace {

using tracing::platform::FormatInputObservation;
using tracing::platform::FormatInputReject;
using tracing::platform::FormatInputSource;
using tracing::platform::GestureMapping;
using tracing::platform::InputDeviceKind;
using tracing::platform::InputObserver;
using tracing::platform::InputReject;
using tracing::platform::InputSample;
using tracing::platform::InputSource;
using tracing::platform::VisualDelta;
using tracing::platform::kMaxPredictionAge;

constexpr std::uintptr_t kHwnd = 0x100;
constexpr std::uint32_t kPid = 42;
constexpr std::uint64_t kGeneration = 7;
constexpr double kPi = 3.14159265358979323846;

GestureMapping ValidMapping()
{
    GestureMapping mapping{};
    mapping.wheel = tracing::platform::MappedAction::Zoom;
    mapping.wheelShift = tracing::platform::MappedAction::Rotate;
    mapping.middleDrag = tracing::platform::MappedAction::Pan;
    mapping.panPixelsPerMouseUnit = 1.0;
    mapping.zoomPerWheelNotch = 1.1;
    mapping.radiansPerWheelNotch = 15.0 * kPi / 180.0;
    return mapping;
}

InputSample MouseSample()
{
    InputSample sample{};
    sample.foregroundHwnd = kHwnd;
    sample.pid = kPid;
    sample.targetGeneration = kGeneration;
    sample.deviceKind = InputDeviceKind::Mouse;
    return sample;
}

bool ArmObserver(InputObserver& observer)
{
    std::string error;
    if (!observer.ApplyMapping(ValidMapping(), error))
    {
        return false;
    }
    observer.AttachSession(kHwnd, kPid, kGeneration);
    observer.Enable();
    return observer.Enabled();
}

} // namespace

TEST(InputPrediction, DisabledByDefaultIgnoresWheelSample)
{
    InputObserver observer;
    auto const now = std::chrono::steady_clock::now();
    InputSample sample = MouseSample();
    sample.hasWheel = true;
    sample.wheelNotches = 1;

    auto const disabled = observer.Observe(sample, now);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_FALSE(observer.MappingApplied());
    EXPECT_EQ(disabled.source, InputSource::Disabled);
    EXPECT_EQ(disabled.reject, InputReject::MappingNotApplied);
    EXPECT_FALSE(disabled.pending);
    EXPECT_FALSE(disabled.hasScale);
    EXPECT_FALSE(disabled.confirmed);
    EXPECT_STREQ(FormatInputSource(disabled.source), "disabled");
}

TEST(InputPrediction, MappingNotAppliedKeepsDisabledAfterEnable)
{
    InputObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    observer.Enable();
    EXPECT_FALSE(observer.Enabled());
    EXPECT_EQ(observer.Last().reject, InputReject::MappingNotApplied);

    auto const now = std::chrono::steady_clock::now();
    InputSample sample = MouseSample();
    sample.hasWheel = true;
    sample.wheelNotches = 1;
    auto const observed = observer.Observe(sample, now);
    EXPECT_EQ(observed.source, InputSource::Disabled);
    EXPECT_FALSE(observed.pending);
    EXPECT_FALSE(observed.hasScale);
}

TEST(InputPrediction, EnabledVersusDisabledSameWheelSample)
{
    InputObserver enabled;
    ASSERT_TRUE(ArmObserver(enabled));
    InputObserver disabled;
    std::string error;
    ASSERT_TRUE(disabled.ApplyMapping(ValidMapping(), error));
    disabled.AttachSession(kHwnd, kPid, kGeneration);

    auto const now = std::chrono::steady_clock::now();
    InputSample sample = MouseSample();
    sample.hasWheel = true;
    sample.wheelNotches = 1;

    auto const predicted = enabled.Observe(sample, now);
    auto const ignored = disabled.Observe(sample, now);

    EXPECT_EQ(predicted.source, InputSource::Predicted);
    EXPECT_TRUE(predicted.pending);
    EXPECT_TRUE(predicted.hasScale);
    EXPECT_NEAR(predicted.logScale, std::log(1.1), 1.0e-12);
    EXPECT_LT(predicted.confidence, 0.5);
    EXPECT_NE(FormatInputObservation(predicted).find("assumedCspApplied=no"), std::string::npos);

    EXPECT_EQ(ignored.source, InputSource::Disabled);
    EXPECT_FALSE(ignored.pending);
    EXPECT_FALSE(ignored.hasScale);
    EXPECT_NE(predicted.pending, ignored.pending);
}

TEST(InputPrediction, UnknownGestureAndNonMouseProduceNoPrediction)
{
    InputObserver observer;
    ASSERT_TRUE(ArmObserver(observer));
    auto const now = std::chrono::steady_clock::now();

    InputSample leftClick = MouseSample();
    leftClick.mouseDx = 4.0;
    auto const unknown = observer.Observe(leftClick, now);
    EXPECT_EQ(unknown.source, InputSource::UnknownGesture);
    EXPECT_EQ(unknown.reject, InputReject::UnknownGesture);
    EXPECT_FALSE(unknown.pending);
    EXPECT_FALSE(unknown.hasTranslation);
    EXPECT_STREQ(FormatInputReject(unknown.reject), "unknown-gesture");

    InputSample pen = MouseSample();
    pen.deviceKind = InputDeviceKind::Other;
    pen.hasWheel = true;
    pen.wheelNotches = 1;
    auto const other = observer.Observe(pen, now);
    EXPECT_EQ(other.source, InputSource::UnknownGesture);
    EXPECT_FALSE(other.pending);
    EXPECT_FALSE(other.hasScale);
}

TEST(InputPrediction, WrongForegroundIgnoresAndDoesNotAssumeCspApplied)
{
    InputObserver observer;
    ASSERT_TRUE(ArmObserver(observer));
    auto const now = std::chrono::steady_clock::now();
    InputSample sample = MouseSample();
    sample.foregroundHwnd = 0x999;
    sample.hasWheel = true;
    sample.wheelNotches = 2;

    auto const observed = observer.Observe(sample, now);
    EXPECT_EQ(observed.source, InputSource::WrongForeground);
    EXPECT_EQ(observed.reject, InputReject::WrongForeground);
    EXPECT_FALSE(observed.pending);
    EXPECT_FALSE(observed.hasScale);
    EXPECT_FALSE(observed.confirmed);
    EXPECT_NE(FormatInputObservation(observed).find("assumedCspApplied=no"), std::string::npos);
}

TEST(InputPrediction, GenerationChangeDropsPending)
{
    InputObserver observer;
    ASSERT_TRUE(ArmObserver(observer));
    auto const t0 = std::chrono::steady_clock::now();
    InputSample wheel = MouseSample();
    wheel.hasWheel = true;
    wheel.wheelNotches = 1;
    auto const predicted = observer.Observe(wheel, t0);
    ASSERT_EQ(predicted.source, InputSource::Predicted);
    ASSERT_TRUE(predicted.pending);

    InputSample next = wheel;
    next.targetGeneration = kGeneration + 1;
    auto const dropped = observer.Observe(next, t0 + std::chrono::milliseconds(10));
    EXPECT_EQ(dropped.source, InputSource::GenerationChanged);
    EXPECT_FALSE(dropped.pending);
    EXPECT_FALSE(dropped.hasScale);
    EXPECT_FALSE(observer.Last().pending);
}

TEST(InputPrediction, PredictionExpiresWithin100msWithoutVisual)
{
    InputObserver observer;
    ASSERT_TRUE(ArmObserver(observer));
    auto const t0 = std::chrono::steady_clock::now();
    InputSample pan = MouseSample();
    pan.middleButtonDown = true;
    pan.mouseDx = 8.0;
    pan.mouseDy = -3.0;
    auto const predicted = observer.Observe(pan, t0);
    ASSERT_EQ(predicted.source, InputSource::Predicted);
    EXPECT_TRUE(predicted.hasTranslation);
    EXPECT_NEAR(predicted.dx, 8.0, 1.0e-12);
    EXPECT_NEAR(predicted.dy, -3.0, 1.0e-12);

    auto const stillLive = observer.Tick(t0 + kMaxPredictionAge - std::chrono::milliseconds(1));
    EXPECT_EQ(stillLive.source, InputSource::Predicted);
    EXPECT_TRUE(stillLive.pending);
    EXPECT_EQ(stillLive.ageMs, 99);

    auto const expired = observer.Tick(t0 + kMaxPredictionAge);
    EXPECT_EQ(expired.source, InputSource::Expired);
    EXPECT_EQ(expired.reject, InputReject::Expired);
    EXPECT_FALSE(expired.pending);
    EXPECT_GE(expired.ageMs, 100);
    EXPECT_FALSE(expired.confirmed);
}

TEST(InputPrediction, ContradictoryVisualDiscardsPending)
{
    InputObserver observer;
    ASSERT_TRUE(ArmObserver(observer));
    auto const t0 = std::chrono::steady_clock::now();
    InputSample pan = MouseSample();
    pan.middleButtonDown = true;
    pan.mouseDx = 5.0;
    pan.mouseDy = 0.0;
    ASSERT_EQ(observer.Observe(pan, t0).source, InputSource::Predicted);

    VisualDelta visual{};
    visual.hasTranslation = true;
    visual.dx = 40.0;
    visual.dy = 0.0;
    visual.targetGeneration = kGeneration;
    auto const contradicted =
        observer.CorrectWithVisual(visual, t0 + std::chrono::milliseconds(20));
    EXPECT_EQ(contradicted.source, InputSource::Contradicted);
    EXPECT_EQ(contradicted.reject, InputReject::Contradicted);
    EXPECT_FALSE(contradicted.pending);
    EXPECT_FALSE(contradicted.confirmed);
}

TEST(InputPrediction, AgreeingVisualConfirmsAndClears)
{
    InputObserver observer;
    ASSERT_TRUE(ArmObserver(observer));
    auto const t0 = std::chrono::steady_clock::now();
    InputSample pan = MouseSample();
    pan.middleButtonDown = true;
    pan.mouseDx = 5.0;
    pan.mouseDy = -2.0;
    ASSERT_EQ(observer.Observe(pan, t0).source, InputSource::Predicted);

    VisualDelta visual{};
    visual.hasTranslation = true;
    visual.dx = 5.2;
    visual.dy = -1.8;
    visual.targetGeneration = kGeneration;
    auto const confirmed = observer.CorrectWithVisual(visual, t0 + std::chrono::milliseconds(20));
    EXPECT_EQ(confirmed.source, InputSource::Confirmed);
    EXPECT_EQ(confirmed.reject, InputReject::Ok);
    EXPECT_FALSE(confirmed.pending);
    EXPECT_TRUE(confirmed.confirmed);
    EXPECT_GT(confirmed.confidence, 0.0);
}

TEST(InputPrediction, RejectsNonFiniteMapping)
{
    InputObserver observer;
    GestureMapping mapping = ValidMapping();
    mapping.zoomPerWheelNotch = 0.0;
    std::string error;
    EXPECT_FALSE(observer.ApplyMapping(mapping, error));
    EXPECT_FALSE(observer.MappingApplied());
    EXPECT_FALSE(error.empty());

    mapping.zoomPerWheelNotch = 1.1;
    mapping.panPixelsPerMouseUnit = std::nan("");
    EXPECT_FALSE(observer.ApplyMapping(mapping, error));
    EXPECT_FALSE(observer.Enabled());
}
