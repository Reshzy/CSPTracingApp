#include "platform/InputObserver.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace tracing::platform {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kPredictionConfidence = 0.25;
constexpr double kConfirmedConfidence = 0.5;

bool IsFiniteValue(double value) noexcept
{
    return std::isfinite(value);
}

double WrapRadians(double radians) noexcept
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

std::int64_t TicksMs(std::chrono::steady_clock::time_point now) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void AppendDouble(std::string& text, char const* format, double value)
{
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), format, value);
    text += buffer;
}

MappedAction ResolveAction(GestureMapping const& mapping, InputSample const& sample) noexcept
{
    if (sample.hasWheel && sample.wheelNotches != 0)
    {
        if (sample.shiftDown)
        {
            return mapping.wheelShift;
        }
        return mapping.wheel;
    }
    if (sample.middleButtonDown && (sample.mouseDx != 0.0 || sample.mouseDy != 0.0))
    {
        return mapping.middleDrag;
    }
    return MappedAction::None;
}

} // namespace

char const* FormatInputSource(InputSource source) noexcept
{
    switch (source)
    {
    case InputSource::Disabled:
        return "disabled";
    case InputSource::UnknownGesture:
        return "unknown-gesture";
    case InputSource::WrongForeground:
        return "wrong-foreground";
    case InputSource::GenerationChanged:
        return "generation-changed";
    case InputSource::Expired:
        return "expired";
    case InputSource::Contradicted:
        return "contradicted";
    case InputSource::Confirmed:
        return "confirmed";
    case InputSource::Predicted:
        return "predicted";
    }
    return "unknown";
}

char const* FormatInputReject(InputReject reject) noexcept
{
    switch (reject)
    {
    case InputReject::Ok:
        return "ok";
    case InputReject::Disabled:
        return "disabled";
    case InputReject::MappingNotApplied:
        return "mapping-not-applied";
    case InputReject::NoSession:
        return "no-session";
    case InputReject::UnknownGesture:
        return "unknown-gesture";
    case InputReject::WrongForeground:
        return "wrong-foreground";
    case InputReject::GenerationChanged:
        return "generation-changed";
    case InputReject::Expired:
        return "expired";
    case InputReject::Contradicted:
        return "contradicted";
    case InputReject::NonFinite:
        return "non-finite";
    }
    return "unknown";
}

std::string FormatInputObservation(InputObservation const& observation)
{
    std::string text = "pred src=";
    text += FormatInputSource(observation.source);
    text += " reject=";
    text += FormatInputReject(observation.reject);
    text += " pending=";
    text += observation.pending ? "yes" : "no";
    text += " confirmed=";
    text += observation.confirmed ? "yes" : "no";
    text += " ageMs=";
    text += std::to_string(observation.ageMs);
    text += " trans=";
    text += observation.hasTranslation ? "yes" : "no";
    text += " scale=";
    text += observation.hasScale ? "yes" : "no";
    text += " rot=";
    text += observation.hasRotation ? "yes" : "no";
    text += " dx=";
    AppendDouble(text, "%.3f", observation.dx);
    text += " dy=";
    AppendDouble(text, "%.3f", observation.dy);
    text += " logS=";
    AppendDouble(text, "%.4f", observation.logScale);
    text += " rotRad=";
    AppendDouble(text, "%.4f", observation.radiansClockwise);
    text += " conf=";
    AppendDouble(text, "%.3f", observation.confidence);
    text += " assumedCspApplied=no";
    return text;
}

InputObserver::InputObserver(InputObserverOptions options)
    : options_(options)
{
}

void InputObserver::AttachSession(
    std::uintptr_t hwnd,
    std::uint32_t pid,
    std::uint64_t generation) noexcept
{
    sessionAttached_ = hwnd != 0 && generation != 0;
    sessionHwnd_ = hwnd;
    sessionPid_ = pid;
    sessionGeneration_ = generation;
    last_.pending = false;
    last_.confirmed = false;
}

void InputObserver::Detach() noexcept
{
    enabled_ = false;
    sessionAttached_ = false;
    sessionHwnd_ = 0;
    sessionPid_ = 0;
    sessionGeneration_ = 0;
    last_ = MakeBase(InputSource::Disabled, InputReject::Disabled, std::chrono::steady_clock::now());
}

bool InputObserver::ApplyMapping(GestureMapping mapping, std::string& error)
{
    if (!IsFiniteValue(mapping.panPixelsPerMouseUnit) ||
        !IsFiniteValue(mapping.zoomPerWheelNotch) ||
        !IsFiniteValue(mapping.radiansPerWheelNotch) ||
        mapping.zoomPerWheelNotch <= 0.0)
    {
        error = "Gesture mapping rejected: pan/zoom/rotate magnitudes must be finite and zoom > 0.";
        mapping_.applied = false;
        enabled_ = false;
        return false;
    }
    mapping.applied = true;
    mapping_ = mapping;
    error.clear();
    return true;
}

void InputObserver::Enable() noexcept
{
    if (!mapping_.applied)
    {
        enabled_ = false;
        last_.source = InputSource::Disabled;
        last_.reject = InputReject::MappingNotApplied;
        last_.pending = false;
        last_.confirmed = false;
        last_.confidence = 0.0;
        ClearPendingComponents(last_);
        return;
    }
    enabled_ = true;
}

void InputObserver::Disable() noexcept
{
    enabled_ = false;
    last_.pending = false;
    last_.confirmed = false;
    last_.source = InputSource::Disabled;
    last_.reject = InputReject::Disabled;
    last_.confidence = 0.0;
    ClearPendingComponents(last_);
}

InputObservation const& InputObserver::Last() const noexcept
{
    return last_;
}

bool InputObserver::Enabled() const noexcept
{
    return enabled_;
}

bool InputObserver::MappingApplied() const noexcept
{
    return mapping_.applied;
}

bool InputObserver::SessionAttached() const noexcept
{
    return sessionAttached_;
}

GestureMapping const& InputObserver::Mapping() const noexcept
{
    return mapping_;
}

InputObservation InputObserver::MakeBase(
    InputSource source,
    InputReject reject,
    std::chrono::steady_clock::time_point now) const
{
    InputObservation observation{};
    observation.source = source;
    observation.reject = reject;
    observation.ticksMs = TicksMs(now);
    observation.targetGeneration = sessionGeneration_;
    return observation;
}

void InputObserver::ClearPendingComponents(InputObservation& observation) const noexcept
{
    observation.hasTranslation = false;
    observation.hasScale = false;
    observation.hasRotation = false;
    observation.dx = 0.0;
    observation.dy = 0.0;
    observation.logScale = 0.0;
    observation.radiansClockwise = 0.0;
    observation.pending = false;
    observation.confirmed = false;
    observation.confidence = 0.0;
    observation.ageMs = 0;
}

InputObservation InputObserver::ExpireIfDue(std::chrono::steady_clock::time_point now)
{
    if (!last_.pending)
    {
        return last_;
    }
    auto const age = std::chrono::duration_cast<std::chrono::milliseconds>(now - predictedAt_);
    last_.ageMs = age.count();
    last_.ticksMs = TicksMs(now);
    if (age >= kMaxPredictionAge)
    {
        last_.source = InputSource::Expired;
        last_.reject = InputReject::Expired;
        last_.pending = false;
        last_.confirmed = false;
        last_.confidence = 0.0;
    }
    return last_;
}

InputObservation InputObserver::Tick(std::chrono::steady_clock::time_point now)
{
    return ExpireIfDue(now);
}

bool InputObserver::PredictionsContradict(VisualDelta const& measured) const noexcept
{
    if (last_.hasTranslation)
    {
        if (!measured.hasTranslation)
        {
            return true;
        }
        double const dx = last_.dx - measured.dx;
        double const dy = last_.dy - measured.dy;
        if (std::hypot(dx, dy) > options_.contradictionTranslationPx)
        {
            return true;
        }
    }
    if (last_.hasScale)
    {
        if (!measured.hasScale || !IsFiniteValue(measured.logScale))
        {
            return true;
        }
        if (std::fabs(last_.logScale - measured.logScale) > options_.contradictionLogScale)
        {
            return true;
        }
    }
    if (last_.hasRotation)
    {
        if (!measured.hasRotation || !IsFiniteValue(measured.radiansClockwise))
        {
            return true;
        }
        if (std::fabs(WrapRadians(last_.radiansClockwise - measured.radiansClockwise)) >
            options_.contradictionRadians)
        {
            return true;
        }
    }
    return false;
}

InputObservation InputObserver::CorrectWithVisual(
    VisualDelta const& measured,
    std::chrono::steady_clock::time_point now)
{
    ExpireIfDue(now);
    if (!last_.pending)
    {
        return last_;
    }
    if (measured.targetGeneration != 0 && measured.targetGeneration != sessionGeneration_)
    {
        last_ = MakeBase(InputSource::GenerationChanged, InputReject::GenerationChanged, now);
        return last_;
    }
    if (PredictionsContradict(measured))
    {
        last_.source = InputSource::Contradicted;
        last_.reject = InputReject::Contradicted;
        last_.pending = false;
        last_.confirmed = false;
        last_.confidence = 0.0;
        last_.ticksMs = TicksMs(now);
        last_.ageMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - predictedAt_).count();
        return last_;
    }

    last_.source = InputSource::Confirmed;
    last_.reject = InputReject::Ok;
    last_.pending = false;
    last_.confirmed = true;
    last_.confidence = kConfirmedConfidence;
    last_.ticksMs = TicksMs(now);
    last_.ageMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - predictedAt_).count();
    return last_;
}

InputObservation InputObserver::Observe(
    InputSample const& sample,
    std::chrono::steady_clock::time_point now)
{
    ExpireIfDue(now);

    if (!enabled_)
    {
        InputReject const reject =
            mapping_.applied ? InputReject::Disabled : InputReject::MappingNotApplied;
        last_ = MakeBase(InputSource::Disabled, reject, now);
        return last_;
    }
    if (!sessionAttached_)
    {
        last_ = MakeBase(InputSource::Disabled, InputReject::NoSession, now);
        return last_;
    }
    if (sample.targetGeneration != sessionGeneration_ ||
        (sample.pid != 0 && sample.pid != sessionPid_))
    {
        last_ = MakeBase(InputSource::GenerationChanged, InputReject::GenerationChanged, now);
        return last_;
    }
    if (sample.foregroundHwnd != sessionHwnd_)
    {
        last_ = MakeBase(InputSource::WrongForeground, InputReject::WrongForeground, now);
        return last_;
    }
    if (sample.deviceKind != InputDeviceKind::Mouse)
    {
        if (!last_.pending)
        {
            last_ = MakeBase(InputSource::UnknownGesture, InputReject::UnknownGesture, now);
        }
        return last_;
    }

    MappedAction const action = ResolveAction(mapping_, sample);
    if (action == MappedAction::None)
    {
        if (!last_.pending)
        {
            last_ = MakeBase(InputSource::UnknownGesture, InputReject::UnknownGesture, now);
        }
        return last_;
    }

    InputObservation predicted = MakeBase(InputSource::Predicted, InputReject::Ok, now);
    predicted.pending = true;
    predicted.confidence = kPredictionConfidence;
    switch (action)
    {
    case MappedAction::Pan:
        predicted.hasTranslation = true;
        predicted.dx = sample.mouseDx * mapping_.panPixelsPerMouseUnit;
        predicted.dy = sample.mouseDy * mapping_.panPixelsPerMouseUnit;
        if (!IsFiniteValue(predicted.dx) || !IsFiniteValue(predicted.dy))
        {
            last_ = MakeBase(InputSource::UnknownGesture, InputReject::NonFinite, now);
            return last_;
        }
        break;
    case MappedAction::Zoom:
    {
        double const factor = std::pow(mapping_.zoomPerWheelNotch, sample.wheelNotches);
        predicted.hasScale = true;
        predicted.logScale = std::log(factor);
        if (!IsFiniteValue(predicted.logScale) || factor <= 0.0)
        {
            last_ = MakeBase(InputSource::UnknownGesture, InputReject::NonFinite, now);
            return last_;
        }
        break;
    }
    case MappedAction::Rotate:
        predicted.hasRotation = true;
        predicted.radiansClockwise =
            static_cast<double>(sample.wheelNotches) * mapping_.radiansPerWheelNotch;
        if (!IsFiniteValue(predicted.radiansClockwise))
        {
            last_ = MakeBase(InputSource::UnknownGesture, InputReject::NonFinite, now);
            return last_;
        }
        break;
    default:
        last_ = MakeBase(InputSource::UnknownGesture, InputReject::UnknownGesture, now);
        return last_;
    }

    predictedAt_ = now;
    predicted.ageMs = 0;
    last_ = predicted;
    return last_;
}

} // namespace tracing::platform
