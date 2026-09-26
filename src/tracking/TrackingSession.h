#pragma once

#include "core/Transform2D.h"
#include "tracking/TransformFusion.h"
#include "tracking/VisualTracker.h"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace tracing::tracking {

// Provisional MASTER.md age limits. Not claimed production tuning.
inline constexpr std::chrono::milliseconds kDegradedAge{150};
inline constexpr std::chrono::milliseconds kLostAge{300};
inline constexpr int kReacquireConsistentCount = 3;
inline constexpr int kParityHysteresisCount = 2;
inline constexpr double kMinSecondaryConfidence = 0.45;
inline constexpr int kMinSecondaryInliers = 16;
inline constexpr std::size_t kMaxTrackingPending = 2;

enum class TrackingState
{
    Unattached,
    Calibrating,
    Tracking,
    Degraded,
    Lost,
    Paused,
    Unavailable,
};

enum class TrackingFrameAction
{
    RejectNotCalibrated,
    RejectWrongGeneration,
    RejectOutOfOrder,
    DropOldestThenEnqueue,
    Enqueue,
};

enum class TrackingApplyAction
{
    Ignored,
    RejectedQuality,
    RejectedContradiction,
    Accepted,
    Reacquired,
};

struct TrackingSessionOptions
{
    std::chrono::milliseconds degradedAge = kDegradedAge;
    std::chrono::milliseconds lostAge = kLostAge;
    int reacquireCount = kReacquireConsistentCount;
    double contradictionTranslationPx = 12.0;
    double contradictionLogScale = 0.05;
    double contradictionRadians = 5.0 * 3.14159265358979323846 / 180.0;
};

struct TrackingFrameMeta
{
    std::uint64_t sequence = 0;
    std::int64_t captureTicks = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    std::uint64_t calibrationGeneration = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    int downsample = 1;
};

struct TrackingRoiFrame
{
    TrackingFrameMeta meta{};
    std::vector<std::uint8_t> bgra;
};

struct TransformSnapshot
{
    core::Transform2D mDs = core::Identity(core::Space::D, core::Space::S);
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    std::uint64_t calibrationGeneration = 0;
    std::uint64_t sequence = 0;
    std::int64_t captureTicks = 0;
    std::int64_t observationAgeMs = 0;
    TrackingState state = TrackingState::Unattached;
    bool hideOverlay = false;
    bool hasKeyframe = false;
    double confidence = 0.0;
    int inlierCount = 0;
    double rmsResidualPx = 0.0;
    VisualReject lastReject = VisualReject::DegenerateSize;
    bool flipX = false;
    bool flipY = false;
    double driftRmsPx = 0.0;
    int reacquireCount = 0;
    bool hasSecondaryAnchor = false;
    FusionMode fusionMode = FusionMode::VisualOnly;
    FusionReject fusionReject = FusionReject::NoUsableSample;
    bool usedVisual = false;
    bool usedNavigator = false;
    bool usedInput = false;
    bool usedAccessibility = false;
    double visualWeight = 0.0;
    double navigatorWeight = 0.0;
    double inputWeight = 0.0;
    double accessibilityWeight = 0.0;
};

struct TrackingHandoffResult
{
    TrackingFrameAction action = TrackingFrameAction::RejectNotCalibrated;
};

struct TrackingApplyResult
{
    TrackingApplyAction action = TrackingApplyAction::Ignored;
    TrackingState state = TrackingState::Unattached;
    bool accepted = false;
    bool usedSecondary = false;
    double originDriftRmsPx = 0.0;
    VisualEstimate poseEstimate{};
};

char const* FormatTrackingState(TrackingState state) noexcept;
char const* FormatTrackingFrameAction(TrackingFrameAction action) noexcept;
char const* FormatTrackingApplyAction(TrackingApplyAction action) noexcept;

inline bool TrackingHidesOverlay(TrackingState state) noexcept
{
    return state == TrackingState::Lost || state == TrackingState::Unavailable;
}

inline bool TrackingAcceptsFrames(TrackingState state) noexcept
{
    switch (state)
    {
    case TrackingState::Calibrating:
    case TrackingState::Tracking:
    case TrackingState::Degraded:
    case TrackingState::Lost:
    case TrackingState::Paused:
        return true;
    default:
        return false;
    }
}

inline double WrapRadians(double radians) noexcept
{
    constexpr double kPi = 3.14159265358979323846;
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

inline core::Transform2D MakeOverlaySimilarity(
    double uniformScale,
    double radiansClockwise,
    double tx,
    double ty)
{
    core::Transform2D const scale =
        core::UniformScale(core::Space::O, core::Space::O, uniformScale);
    core::Transform2D const rotate =
        core::RotateClockwise(core::Space::O, core::Space::O, radiansClockwise);
    core::Transform2D const translate = core::Translate(core::Space::O, core::Space::O, tx, ty);
    std::optional<core::Transform2D> const scaled = core::Compose(scale, rotate);
    if (!scaled.has_value())
    {
        return core::Identity(core::Space::O, core::Space::O);
    }
    return core::Compose(*scaled, translate).value_or(core::Identity(core::Space::O, core::Space::O));
}

inline core::Transform2D SimilarityFromEstimate(VisualEstimate const& estimate, int downsample)
{
    int const ds = downsample < 1 ? 1 : downsample;
    core::Transform2D const similarity = MakeOverlaySimilarity(
        estimate.uniformScale,
        estimate.radiansClockwise,
        estimate.tx * static_cast<double>(ds),
        estimate.ty * static_cast<double>(ds));
    if (!estimate.flipX && !estimate.flipY)
    {
        return similarity;
    }
    core::Transform2D const flip =
        core::Flip(core::Space::O, core::Space::O, estimate.flipX, estimate.flipY);
    return core::Compose(flip, similarity).value_or(similarity);
}

inline bool TransformSimilaritiesContradict(
    core::Transform2D const& left,
    core::Transform2D const& right,
    TrackingSessionOptions const& options) noexcept
{
    if (left.from != core::Space::O || left.to != core::Space::O || right.from != core::Space::O ||
        right.to != core::Space::O)
    {
        return true;
    }
    std::optional<core::Vec2> const originL = core::Apply(left, core::Vec2{});
    std::optional<core::Vec2> const originR = core::Apply(right, core::Vec2{});
    if (!originL.has_value() || !originR.has_value())
    {
        return true;
    }
    double const dx = originL->x - originR->x;
    double const dy = originL->y - originR->y;
    if (std::hypot(dx, dy) > options.contradictionTranslationPx)
    {
        return true;
    }

    double const scaleL = std::hypot(left.matrix.m[0], left.matrix.m[1]);
    double const scaleR = std::hypot(right.matrix.m[0], right.matrix.m[1]);
    if (scaleL <= 1.0e-12 || scaleR <= 1.0e-12)
    {
        return true;
    }
    if (std::fabs(std::log(scaleL / scaleR)) > options.contradictionLogScale)
    {
        return true;
    }

    double const angleL = std::atan2(left.matrix.m[1], left.matrix.m[0]);
    double const angleR = std::atan2(right.matrix.m[1], right.matrix.m[0]);
    return std::fabs(WrapRadians(angleL - angleR)) > options.contradictionRadians;
}

inline VisualParity EstimateParity(VisualEstimate const& estimate) noexcept
{
    return VisualParityFromFlags(estimate.flipX, estimate.flipY);
}

inline bool OriginRejectAllowsSecondaryRescue(VisualReject reject) noexcept
{
    return reject == VisualReject::HighResidual || reject == VisualReject::PoorCoverage;
}

inline bool SecondaryPromotionQualityOk(VisualEstimate const& estimate) noexcept
{
    return estimate.reject == VisualReject::Ok &&
           estimate.confidence >= kMinSecondaryConfidence &&
           estimate.inlierCount >= kMinSecondaryInliers;
}

inline bool OverlayRelativeNearIdentity(core::Transform2D const& relative) noexcept
{
    if (relative.from != core::Space::O || relative.to != core::Space::O)
    {
        return false;
    }
    std::optional<core::Vec2> const origin = core::Apply(relative, core::Vec2{});
    if (!origin.has_value())
    {
        return true;
    }
    if (std::hypot(origin->x, origin->y) >= 1.0)
    {
        return false;
    }
    double const scale = std::hypot(relative.matrix.m[0], relative.matrix.m[1]);
    if (std::fabs(scale - 1.0) >= 0.02)
    {
        return false;
    }
    double const angle = std::atan2(relative.matrix.m[1], relative.matrix.m[0]);
    return std::fabs(WrapRadians(angle)) < 0.02;
}

inline bool EstimatesContradict(
    VisualEstimate const& keyframeEstimate,
    bool hasPrevious,
    VisualEstimate const& previousEstimate,
    core::Transform2D const& lastRelative,
    int downsample,
    TrackingSessionOptions const& options) noexcept
{
    if (keyframeEstimate.reject != VisualReject::Ok)
    {
        return false;
    }
    if (!hasPrevious || previousEstimate.reject != VisualReject::Ok)
    {
        return false;
    }

    core::Transform2D const fromKeyframe = SimilarityFromEstimate(keyframeEstimate, downsample);
    core::Transform2D const step = SimilarityFromEstimate(previousEstimate, downsample);
    std::optional<core::Transform2D> const predicted = core::Compose(lastRelative, step);
    if (!predicted.has_value())
    {
        return true;
    }
    return TransformSimilaritiesContradict(fromKeyframe, *predicted, options);
}

inline TrackingState ApplyAgeLimit(
    TrackingState state,
    bool hasTrustworthy,
    std::int64_t ageMs,
    TrackingSessionOptions const& options) noexcept
{
    if (!hasTrustworthy)
    {
        return state;
    }
    if (state == TrackingState::Unattached || state == TrackingState::Unavailable ||
        state == TrackingState::Paused)
    {
        return state;
    }
    if (ageMs >= options.lostAge.count())
    {
        return TrackingState::Lost;
    }
    if (ageMs >= options.degradedAge.count() &&
        (state == TrackingState::Tracking || state == TrackingState::Calibrating))
    {
        return TrackingState::Degraded;
    }
    return state;
}

inline std::optional<core::Transform2D> ComposeTrackedDocumentToScreen(
    core::Transform2D const& mDo,
    core::Vec2 viewportAnchorS)
{
    if (mDo.from != core::Space::D || mDo.to != core::Space::O)
    {
        return std::nullopt;
    }
    core::Transform2D const toScreen =
        core::Translate(core::Space::O, core::Space::S, viewportAnchorS.x, viewportAnchorS.y);
    return core::Compose(mDo, toScreen);
}

std::string FormatTrackingSnapshot(TransformSnapshot const& snapshot);

// GPU-free generation, age, and contradiction policy.
class TrackingSessionPolicy
{
public:
    TrackingState State() const noexcept;
    bool GenerationValid() const noexcept;
    bool HasKeyframe() const noexcept;
    bool HasTrustworthy() const noexcept;
    std::uint64_t LastAcceptedSequence() const noexcept;
    std::size_t Pending() const noexcept;
    int ReacquireCount() const noexcept;
    int ReacquireEvents() const noexcept;
    VisualReject LastReject() const noexcept;
    core::Transform2D LastRelative() const;
    bool HasSecondaryAnchor() const noexcept;
    core::Transform2D SecondaryRelative() const;
    double OriginDriftRmsPx() const noexcept;
    std::uint64_t TargetGeneration() const noexcept;
    std::uint64_t GeometryGeneration() const noexcept;
    std::uint64_t CalibrationGeneration() const noexcept;

    void BeginCalibrated(
        std::uint64_t targetGeneration,
        std::uint64_t geometryGeneration,
        std::uint64_t calibrationGeneration) noexcept;
    void Pause() noexcept;
    void Resume() noexcept;
    void Resync() noexcept;
    void Detach() noexcept;
    void MarkUnavailable() noexcept;
    void NoteGeometryGeneration(std::uint64_t geometryGeneration) noexcept;

    TrackingHandoffResult Arrive(TrackingFrameMeta const& meta) noexcept;
    void NoteDequeued() noexcept;

    void NoteKeyframeCaptured(
        std::chrono::steady_clock::time_point now,
        std::uint64_t sequence) noexcept;
    TrackingApplyResult ApplyObservation(
        TrackingFrameMeta const& meta,
        VisualEstimate const& keyframeEstimate,
        bool hasPrevious,
        VisualEstimate const& previousEstimate,
        TrackingSessionOptions const& options,
        std::chrono::steady_clock::time_point now,
        bool hasSecondaryEstimate = false,
        VisualEstimate const& secondaryEstimate = {}) noexcept;
    bool PromoteSecondaryAnchor() noexcept;
    void Tick(
        std::chrono::steady_clock::time_point now,
        TrackingSessionOptions const& options) noexcept;

    std::int64_t ObservationAgeMs(std::chrono::steady_clock::time_point now) const noexcept;

private:
    void DropKeyframe() noexcept;
    bool GenerationsMatch(TrackingFrameMeta const& meta) const noexcept;
    bool EstimatesAgreeForReacquire(
        VisualEstimate const& estimate,
        TrackingSessionOptions const& options) const noexcept;
    void ResetParityStreak() noexcept;
    bool ParityHysteresisAllows(
        VisualParity incoming, VisualReject originReject) noexcept;
    TrackingApplyResult FinishAccept(
        TrackingFrameMeta const& meta,
        VisualEstimate const& poseEstimate,
        core::Transform2D const& chosenRelative,
        double originDriftRmsPx,
        bool usedSecondary,
        VisualParity acceptedParity,
        TrackingSessionOptions const& options,
        std::chrono::steady_clock::time_point now) noexcept;

    TrackingState state_ = TrackingState::Unattached;
    bool generationValid_ = false;
    bool hasKeyframe_ = false;
    bool hasTrustworthy_ = false;
    std::uint64_t targetGeneration_ = 0;
    std::uint64_t geometryGeneration_ = 0;
    std::uint64_t calibrationGeneration_ = 0;
    std::uint64_t lastAcceptedSequence_ = 0;
    std::size_t pending_ = 0;
    int reacquireCount_ = 0;
    int reacquireEvents_ = 0;
    VisualReject lastReject_ = VisualReject::DegenerateSize;
    core::Transform2D lastRelative_ = core::Identity(core::Space::O, core::Space::O);
    VisualEstimate lastReacquire_{};
    bool hasReacquireRef_ = false;
    bool hasSecondaryAnchor_ = false;
    core::Transform2D secondaryRelative_ = core::Identity(core::Space::O, core::Space::O);
    VisualParity lastAcceptedParity_ = VisualParity::None;
    VisualParity parityStreakParity_ = VisualParity::None;
    int parityStreak_ = 0;
    double lastOriginDriftRmsPx_ = 0.0;
    std::chrono::steady_clock::time_point lastTrustworthy_{};
};

inline TrackingState TrackingSessionPolicy::State() const noexcept
{
    return state_;
}

inline bool TrackingSessionPolicy::GenerationValid() const noexcept
{
    return generationValid_;
}

inline bool TrackingSessionPolicy::HasKeyframe() const noexcept
{
    return hasKeyframe_;
}

inline bool TrackingSessionPolicy::HasTrustworthy() const noexcept
{
    return hasTrustworthy_;
}

inline std::uint64_t TrackingSessionPolicy::LastAcceptedSequence() const noexcept
{
    return lastAcceptedSequence_;
}

inline std::size_t TrackingSessionPolicy::Pending() const noexcept
{
    return pending_;
}

inline int TrackingSessionPolicy::ReacquireCount() const noexcept
{
    return reacquireCount_;
}

inline int TrackingSessionPolicy::ReacquireEvents() const noexcept
{
    return reacquireEvents_;
}

inline VisualReject TrackingSessionPolicy::LastReject() const noexcept
{
    return lastReject_;
}

inline core::Transform2D TrackingSessionPolicy::LastRelative() const
{
    return lastRelative_;
}

inline bool TrackingSessionPolicy::HasSecondaryAnchor() const noexcept
{
    return hasSecondaryAnchor_;
}

inline core::Transform2D TrackingSessionPolicy::SecondaryRelative() const
{
    return secondaryRelative_;
}

inline double TrackingSessionPolicy::OriginDriftRmsPx() const noexcept
{
    return lastOriginDriftRmsPx_;
}

inline std::uint64_t TrackingSessionPolicy::TargetGeneration() const noexcept
{
    return targetGeneration_;
}

inline std::uint64_t TrackingSessionPolicy::GeometryGeneration() const noexcept
{
    return geometryGeneration_;
}

inline std::uint64_t TrackingSessionPolicy::CalibrationGeneration() const noexcept
{
    return calibrationGeneration_;
}

inline void TrackingSessionPolicy::DropKeyframe() noexcept
{
    hasKeyframe_ = false;
    hasTrustworthy_ = false;
    reacquireCount_ = 0;
    reacquireEvents_ = 0;
    hasReacquireRef_ = false;
    hasSecondaryAnchor_ = false;
    secondaryRelative_ = core::Identity(core::Space::O, core::Space::O);
    lastRelative_ = core::Identity(core::Space::O, core::Space::O);
    lastReject_ = VisualReject::DegenerateSize;
    lastAcceptedParity_ = VisualParity::None;
    ResetParityStreak();
    lastOriginDriftRmsPx_ = 0.0;
}

inline void TrackingSessionPolicy::BeginCalibrated(
    std::uint64_t targetGeneration,
    std::uint64_t geometryGeneration,
    std::uint64_t calibrationGeneration) noexcept
{
    state_ = TrackingState::Calibrating;
    generationValid_ = true;
    targetGeneration_ = targetGeneration;
    geometryGeneration_ = geometryGeneration;
    calibrationGeneration_ = calibrationGeneration;
    lastAcceptedSequence_ = 0;
    pending_ = 0;
    DropKeyframe();
}

inline void TrackingSessionPolicy::Pause() noexcept
{
    if (state_ == TrackingState::Tracking || state_ == TrackingState::Degraded ||
        state_ == TrackingState::Calibrating)
    {
        state_ = TrackingState::Paused;
    }
}

inline void TrackingSessionPolicy::Resume() noexcept
{
    if (state_ != TrackingState::Paused)
    {
        return;
    }
    if (hasTrustworthy_ && hasKeyframe_)
    {
        state_ = TrackingState::Tracking;
        return;
    }
    state_ = TrackingState::Calibrating;
}

inline void TrackingSessionPolicy::Resync() noexcept
{
    if (!generationValid_)
    {
        state_ = TrackingState::Unattached;
        DropKeyframe();
        pending_ = 0;
        return;
    }
    state_ = TrackingState::Calibrating;
    lastAcceptedSequence_ = 0;
    pending_ = 0;
    DropKeyframe();
}

inline void TrackingSessionPolicy::Detach() noexcept
{
    state_ = TrackingState::Unattached;
    generationValid_ = false;
    lastAcceptedSequence_ = 0;
    pending_ = 0;
    DropKeyframe();
}

inline void TrackingSessionPolicy::MarkUnavailable() noexcept
{
    state_ = TrackingState::Unavailable;
    generationValid_ = false;
    pending_ = 0;
    hasTrustworthy_ = false;
}

inline void TrackingSessionPolicy::NoteGeometryGeneration(std::uint64_t geometryGeneration) noexcept
{
    geometryGeneration_ = geometryGeneration;
}

inline bool TrackingSessionPolicy::GenerationsMatch(TrackingFrameMeta const& meta) const noexcept
{
    return generationValid_ && meta.targetGeneration == targetGeneration_ &&
           meta.geometryGeneration == geometryGeneration_ &&
           meta.calibrationGeneration == calibrationGeneration_;
}

inline TrackingHandoffResult TrackingSessionPolicy::Arrive(TrackingFrameMeta const& meta) noexcept
{
    TrackingHandoffResult result{};
    if (!TrackingAcceptsFrames(state_) || !generationValid_)
    {
        result.action = TrackingFrameAction::RejectNotCalibrated;
        return result;
    }
    if (!GenerationsMatch(meta))
    {
        result.action = TrackingFrameAction::RejectWrongGeneration;
        return result;
    }
    if (meta.sequence <= lastAcceptedSequence_)
    {
        result.action = TrackingFrameAction::RejectOutOfOrder;
        return result;
    }
    if (pending_ >= kMaxTrackingPending)
    {
        result.action = TrackingFrameAction::DropOldestThenEnqueue;
        return result;
    }
    ++pending_;
    result.action = TrackingFrameAction::Enqueue;
    return result;
}

inline void TrackingSessionPolicy::NoteDequeued() noexcept
{
    if (pending_ > 0)
    {
        --pending_;
    }
}

inline void TrackingSessionPolicy::NoteKeyframeCaptured(
    std::chrono::steady_clock::time_point now,
    std::uint64_t sequence) noexcept
{
    hasKeyframe_ = true;
    hasTrustworthy_ = true;
    lastTrustworthy_ = now;
    lastRelative_ = core::Identity(core::Space::O, core::Space::O);
    lastReject_ = VisualReject::Ok;
    reacquireCount_ = 0;
    hasReacquireRef_ = false;
    lastAcceptedParity_ = VisualParity::None;
    ResetParityStreak();
    lastOriginDriftRmsPx_ = 0.0;
    if (sequence > lastAcceptedSequence_)
    {
        lastAcceptedSequence_ = sequence;
    }
}

inline bool TrackingSessionPolicy::EstimatesAgreeForReacquire(
    VisualEstimate const& estimate,
    TrackingSessionOptions const& options) const noexcept
{
    if (!hasReacquireRef_ || lastReacquire_.reject != VisualReject::Ok)
    {
        return true;
    }
    core::Transform2D const left = SimilarityFromEstimate(lastReacquire_, 1);
    core::Transform2D const right = SimilarityFromEstimate(estimate, 1);
    return !TransformSimilaritiesContradict(left, right, options);
}

inline void TrackingSessionPolicy::ResetParityStreak() noexcept
{
    parityStreak_ = 0;
    parityStreakParity_ = VisualParity::None;
}

inline bool TrackingSessionPolicy::ParityHysteresisAllows(
    VisualParity incoming, VisualReject originReject) noexcept
{
    if (originReject != VisualReject::Ok || incoming == lastAcceptedParity_)
    {
        ResetParityStreak();
        return true;
    }

    if (parityStreak_ == 0 || parityStreakParity_ != incoming)
    {
        parityStreakParity_ = incoming;
        parityStreak_ = 1;
    }
    else
    {
        ++parityStreak_;
    }
    return parityStreak_ >= kParityHysteresisCount;
}

inline bool TrackingSessionPolicy::PromoteSecondaryAnchor() noexcept
{
    if (state_ != TrackingState::Tracking || !hasKeyframe_)
    {
        return false;
    }
    hasSecondaryAnchor_ = true;
    secondaryRelative_ = lastRelative_;
    return true;
}

inline TrackingApplyResult TrackingSessionPolicy::FinishAccept(
    TrackingFrameMeta const& meta,
    VisualEstimate const& poseEstimate,
    core::Transform2D const& chosenRelative,
    double originDriftRmsPx,
    bool usedSecondary,
    VisualParity acceptedParity,
    TrackingSessionOptions const& options,
    std::chrono::steady_clock::time_point now) noexcept
{
    TrackingApplyResult result{};
    lastRelative_ = chosenRelative;
    lastAcceptedSequence_ = meta.sequence;
    hasTrustworthy_ = true;
    lastTrustworthy_ = now;
    lastReject_ = VisualReject::Ok;
    lastOriginDriftRmsPx_ = originDriftRmsPx;
    lastAcceptedParity_ = acceptedParity;
    ResetParityStreak();
    result.accepted = true;
    result.usedSecondary = usedSecondary;
    result.originDriftRmsPx = originDriftRmsPx;
    result.poseEstimate = poseEstimate;
    result.poseEstimate.reject = VisualReject::Ok;

    if (state_ == TrackingState::Lost)
    {
        if (!EstimatesAgreeForReacquire(poseEstimate, options))
        {
            reacquireCount_ = 1;
        }
        else
        {
            ++reacquireCount_;
        }
        lastReacquire_ = poseEstimate;
        hasReacquireRef_ = true;
        if (reacquireCount_ >= options.reacquireCount)
        {
            state_ = TrackingState::Tracking;
            reacquireCount_ = 0;
            hasReacquireRef_ = false;
            ++reacquireEvents_;
            result.action = TrackingApplyAction::Reacquired;
            result.state = state_;
            return result;
        }
        result.action = TrackingApplyAction::Accepted;
        result.state = state_;
        return result;
    }

    reacquireCount_ = 0;
    hasReacquireRef_ = false;
    if (state_ != TrackingState::Paused)
    {
        state_ = TrackingState::Tracking;
    }
    result.action = TrackingApplyAction::Accepted;
    result.state = state_;
    return result;
}

inline TrackingApplyResult TrackingSessionPolicy::ApplyObservation(
    TrackingFrameMeta const& meta,
    VisualEstimate const& keyframeEstimate,
    bool hasPrevious,
    VisualEstimate const& previousEstimate,
    TrackingSessionOptions const& options,
    std::chrono::steady_clock::time_point now,
    bool hasSecondaryEstimate,
    VisualEstimate const& secondaryEstimate) noexcept
{
    TrackingApplyResult result{};
    result.state = state_;
    if (!TrackingAcceptsFrames(state_) || !generationValid_ || !hasKeyframe_)
    {
        result.action = TrackingApplyAction::Ignored;
        return result;
    }
    if (!GenerationsMatch(meta) || meta.sequence <= lastAcceptedSequence_)
    {
        result.action = TrackingApplyAction::Ignored;
        lastReject_ = keyframeEstimate.reject;
        result.state = state_;
        return result;
    }

    lastReject_ = keyframeEstimate.reject;
    lastOriginDriftRmsPx_ = keyframeEstimate.rmsResidualPx;
    result.originDriftRmsPx = keyframeEstimate.rmsResidualPx;
    int const downsample = meta.downsample < 1 ? 1 : meta.downsample;

    if (state_ == TrackingState::Paused)
    {
        result.action = keyframeEstimate.reject == VisualReject::ParityAmbiguous
                            ? TrackingApplyAction::RejectedQuality
                            : TrackingApplyAction::Ignored;
        result.state = state_;
        return result;
    }

    if (keyframeEstimate.reject == VisualReject::ParityAmbiguous)
    {
        ResetParityStreak();
        Pause();
        result.action = TrackingApplyAction::RejectedQuality;
        result.state = state_;
        return result;
    }

    auto secondaryComposed = [&]() -> std::optional<core::Transform2D> {
        if (!hasSecondaryAnchor_ || !hasSecondaryEstimate ||
            secondaryEstimate.reject != VisualReject::Ok)
        {
            return std::nullopt;
        }
        return core::Compose(
            secondaryRelative_, SimilarityFromEstimate(secondaryEstimate, downsample));
    };

    if (keyframeEstimate.reject != VisualReject::Ok)
    {
        if (OriginRejectAllowsSecondaryRescue(keyframeEstimate.reject))
        {
            std::optional<core::Transform2D> const composed = secondaryComposed();
            if (composed.has_value() &&
                !TransformSimilaritiesContradict(*composed, lastRelative_, options))
            {
                return FinishAccept(
                    meta,
                    secondaryEstimate,
                    *composed,
                    keyframeEstimate.rmsResidualPx,
                    true,
                    lastAcceptedParity_,
                    options,
                    now);
            }
        }
        result.action = TrackingApplyAction::RejectedQuality;
        if (state_ == TrackingState::Tracking || state_ == TrackingState::Calibrating)
        {
            state_ = TrackingState::Degraded;
        }
        state_ = ApplyAgeLimit(state_, hasTrustworthy_, ObservationAgeMs(now), options);
        result.state = state_;
        return result;
    }

    VisualParity const incoming = EstimateParity(keyframeEstimate);
    bool const parityChanged = incoming != lastAcceptedParity_;
    if (!ParityHysteresisAllows(incoming, keyframeEstimate.reject))
    {
        result.action = TrackingApplyAction::RejectedQuality;
        if (state_ == TrackingState::Tracking || state_ == TrackingState::Calibrating)
        {
            state_ = TrackingState::Degraded;
        }
        state_ = ApplyAgeLimit(state_, hasTrustworthy_, ObservationAgeMs(now), options);
        result.state = state_;
        return result;
    }

    if (!parityChanged &&
        EstimatesContradict(
            keyframeEstimate,
            hasPrevious,
            previousEstimate,
            lastRelative_,
            downsample,
            options))
    {
        result.action = TrackingApplyAction::RejectedContradiction;
        if (state_ == TrackingState::Tracking || state_ == TrackingState::Calibrating)
        {
            state_ = TrackingState::Degraded;
        }
        reacquireCount_ = 0;
        hasReacquireRef_ = false;
        ResetParityStreak();
        state_ = ApplyAgeLimit(state_, hasTrustworthy_, ObservationAgeMs(now), options);
        result.state = state_;
        return result;
    }

    core::Transform2D chosen = SimilarityFromEstimate(keyframeEstimate, downsample);
    VisualEstimate poseEstimate = keyframeEstimate;
    bool usedSecondary = false;
    std::optional<core::Transform2D> const composed = secondaryComposed();
    if (composed.has_value() &&
        !TransformSimilaritiesContradict(chosen, *composed, options) &&
        secondaryEstimate.rmsResidualPx + 1.0e-9 < keyframeEstimate.rmsResidualPx)
    {
        chosen = *composed;
        poseEstimate = secondaryEstimate;
        usedSecondary = true;
    }

    return FinishAccept(
        meta,
        poseEstimate,
        chosen,
        keyframeEstimate.rmsResidualPx,
        usedSecondary,
        incoming,
        options,
        now);
}

inline void TrackingSessionPolicy::Tick(
    std::chrono::steady_clock::time_point now,
    TrackingSessionOptions const& options) noexcept
{
    state_ = ApplyAgeLimit(state_, hasTrustworthy_, ObservationAgeMs(now), options);
}

inline std::int64_t TrackingSessionPolicy::ObservationAgeMs(
    std::chrono::steady_clock::time_point now) const noexcept
{
    if (!hasTrustworthy_)
    {
        return 0;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTrustworthy_).count();
}

// Copies ROI frames, runs VisualTracker on a worker, publishes immutable snapshots.
// Stop() invalidates generation, notifies, unlocks, then joins.
class TrackingSession
{
public:
    TrackingSession();
    ~TrackingSession();

    TrackingSession(TrackingSession const&) = delete;
    TrackingSession& operator=(TrackingSession const&) = delete;
    TrackingSession(TrackingSession&&) = delete;
    TrackingSession& operator=(TrackingSession&&) = delete;

    bool BeginCalibrated(
        std::uint64_t targetGeneration,
        std::uint64_t geometryGeneration,
        std::uint64_t calibrationGeneration,
        core::Transform2D const& mDs,
        core::Transform2D const& mSo,
        core::Vec2 viewportAnchorS,
        std::string& error);
    void Pause();
    void Resume();
    void Resync();
    void Detach();
    void MarkUnavailable();
    void NoteGeometryGeneration(std::uint64_t geometryGeneration);
    void SetViewportAnchorS(core::Vec2 viewportAnchorS);
    void SetFusionMode(FusionMode mode);
    FusionMode GetFusionMode() const;
    void SetObserverSamples(
        NavigatorObservation const& navigator,
        platform::InputObservation const& input,
        platform::AccessibilitySnapshot const& accessibility,
        FusionMapping const& mapping);
    void Stop();

    TrackingFrameAction SubmitRoiFrame(TrackingRoiFrame frame);
    TrackingApplyResult SubmitEstimateForTest(
        TrackingFrameMeta const& meta,
        VisualEstimate const& keyframeEstimate,
        bool hasPrevious,
        VisualEstimate const& previousEstimate,
        std::chrono::steady_clock::time_point now,
        bool hasSecondaryEstimate = false,
        VisualEstimate const& secondaryEstimate = {});
    void CaptureKeyframeForTest(
        TrackingFrameMeta const& meta,
        std::chrono::steady_clock::time_point now);
    void PromoteSecondaryAnchorForTest();
    void Tick(std::chrono::steady_clock::time_point now);

    TransformSnapshot Snapshot() const;
    std::string FormatReport() const;

private:
    struct PendingFrame
    {
        TrackingRoiFrame frame;
        bool occupied = false;
    };

    void EnsureWorkerLocked();
    void WorkerLoop();
    void ProcessCurrentFrame(TrackingRoiFrame const& current);
    TransformSnapshot BuildSnapshotLocked(std::chrono::steady_clock::time_point now) const;
    void PublishLocked(std::int64_t captureTicks, std::uint64_t sequence, VisualEstimate const& estimate);
    void ResetFusionLocked() noexcept;
    void FuseAndPublishLocked(
        std::chrono::steady_clock::time_point now,
        TrackingFrameMeta const* visualMeta,
        VisualEstimate const* overlayVisual);

    TrackingSessionOptions options_{};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stop_ = false;
    bool workerStarted_ = false;
    TrackingSessionPolicy policy_;
    core::Transform2D mDoKeyframe_ = core::Identity(core::Space::D, core::Space::O);
    core::Vec2 viewportAnchorS_{};
    TrackingRoiFrame keyframe_{};
    TrackingRoiFrame previous_{};
    TrackingRoiFrame secondary_{};
    bool hasPreviousFrame_ = false;
    bool hasSecondaryFrame_ = false;
    PendingFrame pending_[2]{};
    std::size_t pendingCount_ = 0;
    TransformSnapshot snapshot_{};
    VisualEstimate lastEstimate_{};
    bool pausedForParity_ = false;
    TransformFusion fusion_{};
    FusionMode fusionMode_ = FusionMode::VisualOnly;
    bool hasNavigatorSample_ = false;
    NavigatorObservation navigatorSample_{};
    bool hasInputSample_ = false;
    platform::InputObservation inputSample_{};
    bool hasAccessibilitySample_ = false;
    platform::AccessibilitySnapshot accessibilitySample_{};
    FusionMapping fusionMapping_{};
    bool hasLastOverlayVisual_ = false;
    FusionSample lastOverlayVisual_{};
    bool hasFusedRelative_ = false;
    core::Transform2D fusedRelative_ = core::Identity(core::Space::O, core::Space::O);
    FusionResult lastFusion_{};
};

} // namespace tracing::tracking
