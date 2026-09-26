#include "tracking/TrackingSession.h"

#include <cstdio>
#include <utility>

namespace tracing::tracking {
namespace {

bool FramePixelsUsable(TrackingRoiFrame const& frame) noexcept
{
    if (frame.meta.width <= 0 || frame.meta.height <= 0 || frame.meta.stride <= 0)
    {
        return false;
    }
    std::size_t const needed = static_cast<std::size_t>(frame.meta.stride) *
                               static_cast<std::size_t>(frame.meta.height);
    return frame.bgra.size() >= needed;
}

CanvasView ViewFromFrame(TrackingRoiFrame const& frame) noexcept
{
    CanvasView view{};
    view.data = frame.bgra.data();
    view.width = frame.meta.width;
    view.height = frame.meta.height;
    view.stride = frame.meta.stride;
    view.format = PixelFormat::Bgra32;
    return view;
}

} // namespace

char const* FormatTrackingState(TrackingState state) noexcept
{
    switch (state)
    {
    case TrackingState::Unattached:
        return "Unattached";
    case TrackingState::Calibrating:
        return "Calibrating";
    case TrackingState::Tracking:
        return "Tracking";
    case TrackingState::Degraded:
        return "Degraded";
    case TrackingState::Lost:
        return "Lost";
    case TrackingState::Paused:
        return "Paused";
    case TrackingState::Unavailable:
        return "Unavailable";
    }
    return "Unknown";
}

char const* FormatTrackingFrameAction(TrackingFrameAction action) noexcept
{
    switch (action)
    {
    case TrackingFrameAction::RejectNotCalibrated:
        return "reject-not-calibrated";
    case TrackingFrameAction::RejectWrongGeneration:
        return "reject-wrong-generation";
    case TrackingFrameAction::RejectOutOfOrder:
        return "reject-out-of-order";
    case TrackingFrameAction::DropOldestThenEnqueue:
        return "drop-oldest-then-enqueue";
    case TrackingFrameAction::Enqueue:
        return "enqueue";
    }
    return "unknown";
}

char const* FormatTrackingApplyAction(TrackingApplyAction action) noexcept
{
    switch (action)
    {
    case TrackingApplyAction::Ignored:
        return "ignored";
    case TrackingApplyAction::RejectedQuality:
        return "rejected-quality";
    case TrackingApplyAction::RejectedContradiction:
        return "rejected-contradiction";
    case TrackingApplyAction::Accepted:
        return "accepted";
    case TrackingApplyAction::Reacquired:
        return "reacquired";
    }
    return "unknown";
}

std::string FormatTrackingSnapshot(TransformSnapshot const& snapshot)
{
    std::string text = "tracking state=";
    text += FormatTrackingState(snapshot.state);
    text += " hide=";
    text += snapshot.hideOverlay ? "yes" : "no";
    text += " conf=";
    char number[64]{};
    std::snprintf(number, sizeof(number), "%.3f", snapshot.confidence);
    text += number;
    text += " ageMs=";
    text += std::to_string(snapshot.observationAgeMs);
    text += " seq=";
    text += std::to_string(snapshot.sequence);
    text += " reject=";
    text += VisualRejectName(snapshot.lastReject);
    text += " keyframe=";
    text += snapshot.hasKeyframe ? "yes" : "no";
    text += " inliers=";
    text += std::to_string(snapshot.inlierCount);
    text += " rms=";
    std::snprintf(number, sizeof(number), "%.3f", snapshot.rmsResidualPx);
    text += number;
    text += " driftRms=";
    std::snprintf(number, sizeof(number), "%.3f", snapshot.driftRmsPx);
    text += number;
    text += " reacquire=";
    text += std::to_string(snapshot.reacquireCount);
    text += " secondary=";
    text += snapshot.hasSecondaryAnchor ? "yes" : "no";
    text += " parity=";
    text += VisualParityName(VisualParityFromFlags(snapshot.flipX, snapshot.flipY));
    text += " gens=";
    text += std::to_string(snapshot.targetGeneration);
    text += "/";
    text += std::to_string(snapshot.geometryGeneration);
    text += "/";
    text += std::to_string(snapshot.calibrationGeneration);
    text += " (provisional 150/300ms age)";
    return text;
}

TrackingSession::TrackingSession() = default;

TrackingSession::~TrackingSession()
{
    Stop();
}

void TrackingSession::EnsureWorkerLocked()
{
    if (workerStarted_ || stop_)
    {
        return;
    }
    workerStarted_ = true;
    worker_ = std::thread(&TrackingSession::WorkerLoop, this);
}

bool TrackingSession::BeginCalibrated(
    std::uint64_t targetGeneration,
    std::uint64_t geometryGeneration,
    std::uint64_t calibrationGeneration,
    core::Transform2D const& mDs,
    core::Transform2D const& mSo,
    core::Vec2 viewportAnchorS,
    std::string& error)
{
    error.clear();
    std::optional<core::Transform2D> const mDo = core::Compose(mDs, mSo);
    if (!mDo.has_value() || mDo->from != core::Space::D || mDo->to != core::Space::O ||
        !core::IsFinite(*mDo) || core::IsSingular(*mDo))
    {
        error = "BeginCalibrated: M_SO * M_DS is not a finite D->O transform";
        return false;
    }

    {
        std::lock_guard<std::mutex> const lock(mutex_);
        policy_.BeginCalibrated(targetGeneration, geometryGeneration, calibrationGeneration);
        mDoKeyframe_ = *mDo;
        viewportAnchorS_ = viewportAnchorS;
        keyframe_ = {};
        previous_ = {};
        secondary_ = {};
        hasPreviousFrame_ = false;
        hasSecondaryFrame_ = false;
        pending_[0] = {};
        pending_[1] = {};
        pendingCount_ = 0;
        lastEstimate_ = {};
        pausedForParity_ = false;
        snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
        stop_ = false;
        EnsureWorkerLocked();
    }
    cv_.notify_all();
    return true;
}

void TrackingSession::Pause()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.Pause();
    snapshot_.state = policy_.State();
    snapshot_.hideOverlay = TrackingHidesOverlay(snapshot_.state);
}

void TrackingSession::Resume()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.Resume();
    pausedForParity_ = false;
    snapshot_.state = policy_.State();
    snapshot_.hideOverlay = TrackingHidesOverlay(snapshot_.state);
}

void TrackingSession::Resync()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.Resync();
    keyframe_ = {};
    previous_ = {};
    secondary_ = {};
    hasPreviousFrame_ = false;
    hasSecondaryFrame_ = false;
    pending_[0] = {};
    pending_[1] = {};
    pendingCount_ = 0;
    lastEstimate_ = {};
    pausedForParity_ = false;
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

void TrackingSession::Detach()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.Detach();
    keyframe_ = {};
    previous_ = {};
    secondary_ = {};
    hasPreviousFrame_ = false;
    hasSecondaryFrame_ = false;
    pending_[0] = {};
    pending_[1] = {};
    pendingCount_ = 0;
    lastEstimate_ = {};
    pausedForParity_ = false;
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

void TrackingSession::MarkUnavailable()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.MarkUnavailable();
    pending_[0] = {};
    pending_[1] = {};
    pendingCount_ = 0;
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

void TrackingSession::NoteGeometryGeneration(std::uint64_t geometryGeneration)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.NoteGeometryGeneration(geometryGeneration);
    snapshot_.geometryGeneration = geometryGeneration;
}

void TrackingSession::SetViewportAnchorS(core::Vec2 viewportAnchorS)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    viewportAnchorS_ = viewportAnchorS;
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

void TrackingSession::Stop()
{
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        stop_ = true;
        policy_.Detach();
        pending_[0] = {};
        pending_[1] = {};
        pendingCount_ = 0;
        snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
        cv_.notify_all();
    }
    if (worker_.joinable())
    {
        worker_.join();
    }
    workerStarted_ = false;
}

TrackingFrameAction TrackingSession::SubmitRoiFrame(TrackingRoiFrame frame)
{
    if (!FramePixelsUsable(frame))
    {
        return TrackingFrameAction::RejectNotCalibrated;
    }

    TrackingHandoffResult result{};
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        result = policy_.Arrive(frame.meta);
        if (result.action == TrackingFrameAction::RejectNotCalibrated ||
            result.action == TrackingFrameAction::RejectWrongGeneration ||
            result.action == TrackingFrameAction::RejectOutOfOrder)
        {
            return result.action;
        }
        if (result.action == TrackingFrameAction::DropOldestThenEnqueue)
        {
            if (pendingCount_ > 1)
            {
                pending_[0] = std::move(pending_[1]);
            }
            pending_[1].frame = std::move(frame);
            pending_[1].occupied = true;
            pendingCount_ = 2;
        }
        else
        {
            std::size_t const slot = pendingCount_;
            pending_[slot].frame = std::move(frame);
            pending_[slot].occupied = true;
            ++pendingCount_;
        }
    }
    cv_.notify_one();
    return result.action;
}

TrackingApplyResult TrackingSession::SubmitEstimateForTest(
    TrackingFrameMeta const& meta,
    VisualEstimate const& keyframeEstimate,
    bool hasPrevious,
    VisualEstimate const& previousEstimate,
    std::chrono::steady_clock::time_point now,
    bool hasSecondaryEstimate,
    VisualEstimate const& secondaryEstimate)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    TrackingApplyResult const applied = policy_.ApplyObservation(
        meta,
        keyframeEstimate,
        hasPrevious,
        previousEstimate,
        options_,
        now,
        hasSecondaryEstimate,
        secondaryEstimate);
    if (applied.accepted)
    {
        PublishLocked(meta.captureTicks, meta.sequence, applied.poseEstimate);
    }
    snapshot_ = BuildSnapshotLocked(now);
    snapshot_.lastReject = policy_.LastReject();
    if (applied.accepted)
    {
        snapshot_.confidence = applied.poseEstimate.confidence;
        snapshot_.inlierCount = applied.poseEstimate.inlierCount;
        snapshot_.rmsResidualPx = applied.poseEstimate.rmsResidualPx;
        snapshot_.sequence = meta.sequence;
        snapshot_.captureTicks = meta.captureTicks;
        snapshot_.lastReject = applied.poseEstimate.reject;
        snapshot_.flipX = applied.poseEstimate.flipX;
        snapshot_.flipY = applied.poseEstimate.flipY;
    }
    if (policy_.State() == TrackingState::Paused &&
        keyframeEstimate.reject == VisualReject::ParityAmbiguous)
    {
        pausedForParity_ = true;
    }
    return applied;
}

void TrackingSession::CaptureKeyframeForTest(
    TrackingFrameMeta const& meta,
    std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    TrackingHandoffResult const arrived = policy_.Arrive(meta);
    if (arrived.action != TrackingFrameAction::Enqueue &&
        arrived.action != TrackingFrameAction::DropOldestThenEnqueue)
    {
        return;
    }
    policy_.NoteDequeued();
    policy_.NoteKeyframeCaptured(now, meta.sequence);
    keyframe_.meta = meta;
    previous_.meta = meta;
    secondary_ = {};
    hasPreviousFrame_ = false;
    hasSecondaryFrame_ = false;
    pausedForParity_ = false;
    lastEstimate_.reject = VisualReject::Ok;
    lastEstimate_.uniformScale = 1.0;
    lastEstimate_.confidence = 1.0;
    lastEstimate_.flipX = false;
    lastEstimate_.flipY = false;
    lastEstimate_.parity = VisualParity::None;
    PublishLocked(meta.captureTicks, meta.sequence, lastEstimate_);
    snapshot_ = BuildSnapshotLocked(now);
}

void TrackingSession::PromoteSecondaryAnchorForTest()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.PromoteSecondaryAnchor();
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

void TrackingSession::Tick(std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    // Age limits apply only when CV is not in flight. ORB on a live ROI can
    // exceed the provisional 300 ms Lost threshold while a frame is pending.
    if (policy_.Pending() == 0)
    {
        policy_.Tick(now, options_);
    }
    snapshot_ = BuildSnapshotLocked(now);
}

TransformSnapshot TrackingSession::Snapshot() const
{
    std::lock_guard<std::mutex> const lock(mutex_);
    return snapshot_;
}

std::string TrackingSession::FormatReport() const
{
    return FormatTrackingSnapshot(Snapshot());
}

void TrackingSession::WorkerLoop()
{
    for (;;)
    {
        TrackingRoiFrame current{};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stop_ || pendingCount_ > 0; });
            if (stop_)
            {
                return;
            }
            while (pendingCount_ > 1)
            {
                pending_[0] = std::move(pending_[1]);
                pending_[1] = {};
                pendingCount_ = 1;
                policy_.NoteDequeued();
            }
            current = std::move(pending_[0].frame);
            pending_[0] = {};
            pendingCount_ = 0;
        }
        ProcessCurrentFrame(current);
        {
            std::lock_guard<std::mutex> const lock(mutex_);
            policy_.NoteDequeued();
        }
    }
}

void TrackingSession::ProcessCurrentFrame(TrackingRoiFrame const& current)
{
    TrackingRoiFrame keyframeCopy{};
    TrackingRoiFrame previousCopy{};
    TrackingRoiFrame secondaryCopy{};
    bool hasPrevious = false;
    bool hasSecondary = false;
    VisualTrackerOptions trackerOptions{};
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        if (!policy_.HasKeyframe())
        {
            keyframe_ = current;
            previous_ = {};
            secondary_ = {};
            hasPreviousFrame_ = false;
            hasSecondaryFrame_ = false;
            pausedForParity_ = false;
            policy_.NoteKeyframeCaptured(std::chrono::steady_clock::now(), current.meta.sequence);
            lastEstimate_.reject = VisualReject::Ok;
            lastEstimate_.uniformScale = 1.0;
            lastEstimate_.confidence = 1.0;
            lastEstimate_.flipX = false;
            lastEstimate_.flipY = false;
            lastEstimate_.parity = VisualParity::None;
            PublishLocked(current.meta.captureTicks, current.meta.sequence, lastEstimate_);
            return;
        }
        keyframeCopy = keyframe_;
        hasPrevious = hasPreviousFrame_;
        if (hasPrevious)
        {
            previousCopy = previous_;
        }
        hasSecondary = hasSecondaryFrame_ && policy_.HasSecondaryAnchor();
        if (hasSecondary)
        {
            secondaryCopy = secondary_;
        }
        trackerOptions.preferredParity =
            VisualParityFromFlags(lastEstimate_.flipX, lastEstimate_.flipY);
    }

    VisualEstimate const keyframeEstimate =
        EstimateCanvasMotion(ViewFromFrame(keyframeCopy), ViewFromFrame(current), trackerOptions);
    VisualEstimate previousEstimate{};
    if (hasPrevious)
    {
        previousEstimate =
            EstimateCanvasMotion(ViewFromFrame(previousCopy), ViewFromFrame(current), trackerOptions);
    }
    VisualEstimate secondaryEstimate{};
    if (hasSecondary)
    {
        secondaryEstimate = EstimateCanvasMotion(
            ViewFromFrame(secondaryCopy), ViewFromFrame(current), trackerOptions);
    }

    auto const now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> const lock(mutex_);
        TrackingApplyResult const applied = policy_.ApplyObservation(
            current.meta,
            keyframeEstimate,
            hasPrevious,
            previousEstimate,
            options_,
            now,
            hasSecondary,
            secondaryEstimate);
        if (applied.accepted)
        {
            PublishLocked(current.meta.captureTicks, current.meta.sequence, applied.poseEstimate);
            if (policy_.State() == TrackingState::Tracking &&
                SecondaryPromotionQualityOk(applied.poseEstimate) &&
                !OverlayRelativeNearIdentity(policy_.LastRelative()))
            {
                secondary_ = current;
                hasSecondaryFrame_ = true;
                policy_.PromoteSecondaryAnchor();
            }
        }
        else
        {
            snapshot_ = BuildSnapshotLocked(now);
            snapshot_.lastReject = policy_.LastReject();
        }
        if (policy_.State() == TrackingState::Paused &&
            keyframeEstimate.reject == VisualReject::ParityAmbiguous)
        {
            pausedForParity_ = true;
        }
        previous_ = current;
        hasPreviousFrame_ = true;
    }
}

TransformSnapshot TrackingSession::BuildSnapshotLocked(
    std::chrono::steady_clock::time_point now) const
{
    TransformSnapshot snapshot{};
    snapshot.state = policy_.State();
    snapshot.hideOverlay = TrackingHidesOverlay(snapshot.state);
    snapshot.hasKeyframe = policy_.HasKeyframe();
    snapshot.observationAgeMs = policy_.ObservationAgeMs(now);
    snapshot.lastReject = policy_.LastReject();
    snapshot.targetGeneration = policy_.TargetGeneration();
    snapshot.geometryGeneration = policy_.GeometryGeneration();
    snapshot.calibrationGeneration = policy_.CalibrationGeneration();
    snapshot.sequence = snapshot_.sequence;
    snapshot.captureTicks = snapshot_.captureTicks;
    snapshot.confidence = snapshot_.confidence;
    snapshot.inlierCount = snapshot_.inlierCount;
    snapshot.rmsResidualPx = snapshot_.rmsResidualPx;
    snapshot.flipX = lastEstimate_.flipX;
    snapshot.flipY = lastEstimate_.flipY;
    snapshot.driftRmsPx = policy_.OriginDriftRmsPx();
    snapshot.reacquireCount = policy_.ReacquireEvents();
    snapshot.hasSecondaryAnchor = policy_.HasSecondaryAnchor();

    core::Transform2D const relative = policy_.LastRelative();
    std::optional<core::Transform2D> const mDo = core::Compose(mDoKeyframe_, relative);
    if (mDo.has_value())
    {
        std::optional<core::Transform2D> const mDs =
            ComposeTrackedDocumentToScreen(*mDo, viewportAnchorS_);
        if (mDs.has_value())
        {
            snapshot.mDs = *mDs;
        }
    }
    return snapshot;
}

void TrackingSession::PublishLocked(
    std::int64_t captureTicks,
    std::uint64_t sequence,
    VisualEstimate const& estimate)
{
    lastEstimate_ = estimate;
    snapshot_.sequence = sequence;
    snapshot_.captureTicks = captureTicks;
    snapshot_.confidence = estimate.confidence;
    snapshot_.inlierCount = estimate.inlierCount;
    snapshot_.rmsResidualPx = estimate.rmsResidualPx;
    snapshot_.lastReject = estimate.reject;
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

} // namespace tracing::tracking
