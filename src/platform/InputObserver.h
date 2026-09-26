#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace tracing::platform {

// Optional Raw Input predictor. GPU-free, no Transform2D, no overlay motion.
// Reports short-lived pan/zoom/rotate components only. Does not swallow,
// inject, or store a raw HID stream. Fusion is a later step.

inline constexpr std::chrono::milliseconds kMaxPredictionAge{100};

enum class InputDeviceKind
{
    Mouse,
    Other,
};

enum class MappedAction
{
    None,
    Pan,
    Zoom,
    Rotate,
};

enum class InputSource
{
    Disabled,
    UnknownGesture,
    WrongForeground,
    GenerationChanged,
    Expired,
    Contradicted,
    Confirmed,
    Predicted,
};

enum class InputReject
{
    Ok,
    Disabled,
    MappingNotApplied,
    NoSession,
    UnknownGesture,
    WrongForeground,
    GenerationChanged,
    Expired,
    Contradicted,
    NonFinite,
};

struct GestureMapping
{
    bool applied = false;
    MappedAction wheel = MappedAction::Zoom;
    MappedAction wheelShift = MappedAction::Rotate;
    MappedAction middleDrag = MappedAction::Pan;
    double panPixelsPerMouseUnit = 1.0;
    double zoomPerWheelNotch = 1.1;
    double radiansPerWheelNotch = 15.0 * 3.14159265358979323846 / 180.0;
};

struct InputSample
{
    std::uintptr_t foregroundHwnd = 0;
    std::uint32_t pid = 0;
    std::uint64_t targetGeneration = 0;
    InputDeviceKind deviceKind = InputDeviceKind::Other;
    bool shiftDown = false;
    bool middleButtonDown = false;
    bool hasWheel = false;
    int wheelNotches = 0;
    double mouseDx = 0.0;
    double mouseDy = 0.0;
};

struct VisualDelta
{
    bool hasTranslation = false;
    bool hasScale = false;
    bool hasRotation = false;
    double dx = 0.0;
    double dy = 0.0;
    double logScale = 0.0;
    double radiansClockwise = 0.0;
    std::uint64_t targetGeneration = 0;
};

struct InputObservation
{
    InputSource source = InputSource::Disabled;
    InputReject reject = InputReject::Disabled;
    std::int64_t ticksMs = 0;
    std::uint64_t targetGeneration = 0;
    bool hasTranslation = false;
    bool hasScale = false;
    bool hasRotation = false;
    double dx = 0.0;
    double dy = 0.0;
    double logScale = 0.0;
    double radiansClockwise = 0.0;
    double confidence = 0.0;
    bool pending = false;
    bool confirmed = false;
    std::int64_t ageMs = 0;
};

struct InputObserverOptions
{
    double contradictionTranslationPx = 12.0;
    double contradictionLogScale = 0.05;
    double contradictionRadians = 5.0 * 3.14159265358979323846 / 180.0;
};

char const* FormatInputSource(InputSource source) noexcept;
char const* FormatInputReject(InputReject reject) noexcept;
std::string FormatInputObservation(InputObservation const& observation);

class InputObserver
{
public:
    explicit InputObserver(InputObserverOptions options = {});

    void AttachSession(std::uintptr_t hwnd, std::uint32_t pid, std::uint64_t generation) noexcept;
    void Detach() noexcept;
    bool ApplyMapping(GestureMapping mapping, std::string& error);
    void Enable() noexcept;
    void Disable() noexcept;

    InputObservation Observe(
        InputSample const& sample,
        std::chrono::steady_clock::time_point now);
    InputObservation Tick(std::chrono::steady_clock::time_point now);
    InputObservation CorrectWithVisual(
        VisualDelta const& measured,
        std::chrono::steady_clock::time_point now);

    InputObservation const& Last() const noexcept;
    bool Enabled() const noexcept;
    bool MappingApplied() const noexcept;
    bool SessionAttached() const noexcept;
    GestureMapping const& Mapping() const noexcept;

private:
    InputObservation MakeBase(
        InputSource source,
        InputReject reject,
        std::chrono::steady_clock::time_point now) const;
    InputObservation ExpireIfDue(std::chrono::steady_clock::time_point now);
    void ClearPendingComponents(InputObservation& observation) const noexcept;
    bool PredictionsContradict(VisualDelta const& measured) const noexcept;

    InputObserverOptions options_{};
    GestureMapping mapping_{};
    bool enabled_ = false;
    bool sessionAttached_ = false;
    std::uintptr_t sessionHwnd_ = 0;
    std::uint32_t sessionPid_ = 0;
    std::uint64_t sessionGeneration_ = 0;
    InputObservation last_{};
    std::chrono::steady_clock::time_point predictedAt_{};
};

} // namespace tracing::platform
