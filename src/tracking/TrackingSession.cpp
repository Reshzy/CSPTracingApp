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

std::int64_t NowMs(std::chrono::steady_clock::time_point now) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

VisualEstimate OverlaySpaceEstimate(VisualEstimate estimate, int downsample) noexcept
{
    int const ds = downsample < 1 ? 1 : downsample;
    estimate.tx *= static_cast<double>(ds);
    estimate.ty *= static_cast<double>(ds);
    return estimate;
}

VisualEstimate IdentityOverlayEstimate() noexcept
{
    VisualEstimate estimate{};
    estimate.reject = VisualReject::Ok;
    estimate.uniformScale = 1.0;
    estimate.confidence = 1.0;
    estimate.inlierCount = 0;
    estimate.parity = VisualParity::None;
    return estimate;
}

VisualEstimate EstimateFromFusion(FusionResult const& fused, VisualEstimate const& quality) noexcept
{
    VisualEstimate estimate = quality;
    estimate.reject = VisualReject::Ok;
    estimate.tx = fused.tx;
    estimate.ty = fused.ty;
    estimate.uniformScale = fused.uniformScale;
    estimate.radiansClockwise = fused.radiansClockwise;
    estimate.flipX = fused.flipX;
    estimate.flipY = fused.flipY;
    estimate.parity = VisualParityFromFlags(fused.flipX, fused.flipY);
    estimate.confidence = fused.confidence;
    return estimate;
}

core::Transform2D RelativeFromFusion(FusionResult const& fused)
{
    VisualEstimate pose{};
    pose.reject = VisualReject::Ok;
    pose.tx = fused.tx;
    pose.ty = fused.ty;
    pose.uniformScale = fused.uniformScale;
    pose.radiansClockwise = fused.radiansClockwise;
    pose.flipX = fused.flipX;
    pose.flipY = fused.flipY;
    return SimilarityFromEstimate(pose, 1);
}

void StampSampleGenerations(FusionSample& sample, FusionContext const& context) noexcept
{
    if (sample.targetGeneration == 0)
    {
        sample.targetGeneration = context.targetGeneration;
    }
    if (sample.geometryGeneration == 0)
    {
        sample.geometryGeneration = context.geometryGeneration;
    }
    if (sample.calibrationGeneration == 0)
    {
        sample.calibrationGeneration = context.calibrationGeneration;
    }
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
    text += " fusion=";
    text += FormatFusionMode(snapshot.fusionMode);
    text += " fuseReject=";
    text += FormatFusionReject(snapshot.fusionReject);
    text += " src=";
    text += snapshot.usedVisual ? "V" : "-";
    text += snapshot.usedNavigator ? "N" : "-";
    text += snapshot.usedInput ? "I" : "-";
    text += snapshot.usedAccessibility ? "U" : "-";
    text += " wV=";
    std::snprintf(number, sizeof(number), "%.3f", snapshot.visualWeight);
    text += number;
    text += " wN=";
    std::snprintf(number, sizeof(number), "%.3f", snapshot.navigatorWeight);
    text += number;
    text += " wI=";
    std::snprintf(number, sizeof(number), "%.3f", snapshot.inputWeight);
    text += number;
    text += " wU=";
    std::snprintf(number, sizeof(number), "%.3f", snapshot.accessibilityWeight);
    text += number;
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
        ResetFusionLocked();
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
    ResetFusionLocked();
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
    ResetFusionLocked();
    snapshot_ = BuildSnapshotLocked(std::chrono::steady_clock::now());
}

void TrackingSession::MarkUnavailable()
{
    std::lock_guard<std::mutex> const lock(mutex_);
    policy_.MarkUnavailable();
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
    ResetFusionLocked();
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

void TrackingSession::SetFusionMode(FusionMode mode)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    fusionMode_ = mode;
    snapshot_.fusionMode = mode;
}

FusionMode TrackingSession::GetFusionMode() const
{
    std::lock_guard<std::mutex> const lock(mutex_);
    return fusionMode_;
}

void TrackingSession::SetObserverSamples(
    NavigatorObservation const& navigator,
    platform::InputObservation const& input,
    platform::AccessibilitySnapshot const& accessibility,
    FusionMapping const& mapping)
{
    std::lock_guard<std::mutex> const lock(mutex_);
    hasNavigatorSample_ = true;
    navigatorSample_ = navigator;
    hasInputSample_ = true;
    inputSample_ = input;
    hasAccessibilitySample_ = true;
    accessibilitySample_ = accessibility;
    fusionMapping_ = mapping;
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
        ResetFusionLocked();
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
        VisualEstimate const overlay = OverlaySpaceEstimate(applied.poseEstimate, meta.downsample);
        FuseAndPublishLocked(now, &meta, &overlay);
        snapshot_.inlierCount = applied.poseEstimate.inlierCount;
        snapshot_.rmsResidualPx = applied.poseEstimate.rmsResidualPx;
        snapshot_.lastReject = applied.poseEstimate.reject;
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
    lastEstimate_ = IdentityOverlayEstimate();
    VisualEstimate const overlay = lastEstimate_;
    FuseAndPublishLocked(now, &meta, &overlay);
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
    if (fusionMode_ == FusionMode::Hybrid && policy_.HasKeyframe())
    {
        FuseAndPublishLocked(now, nullptr, nullptr);
        return;
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
            auto const now = std::chrono::steady_clock::now();
            policy_.NoteKeyframeCaptured(now, current.meta.sequence);
            lastEstimate_ = IdentityOverlayEstimate();
            VisualEstimate const overlay = lastEstimate_;
            FuseAndPublishLocked(now, &current.meta, &overlay);
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
            VisualEstimate const overlay =
                OverlaySpaceEstimate(applied.poseEstimate, current.meta.downsample);
            FuseAndPublishLocked(now, &current.meta, &overlay);
            snapshot_.inlierCount = applied.poseEstimate.inlierCount;
            snapshot_.rmsResidualPx = applied.poseEstimate.rmsResidualPx;
            snapshot_.lastReject = applied.poseEstimate.reject;
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
    snapshot.fusionMode = fusionMode_;
    snapshot.fusionReject = lastFusion_.reject;
    snapshot.usedVisual = lastFusion_.usedVisual;
    snapshot.usedNavigator = lastFusion_.usedNavigator;
    snapshot.usedInput = lastFusion_.usedInput;
    snapshot.usedAccessibility = lastFusion_.usedAccessibility;
    snapshot.visualWeight = lastFusion_.visualWeight;
    snapshot.navigatorWeight = lastFusion_.navigatorWeight;
    snapshot.inputWeight = lastFusion_.inputWeight;
    snapshot.accessibilityWeight = lastFusion_.accessibilityWeight;

    core::Transform2D const relative =
        hasFusedRelative_ ? fusedRelative_ : policy_.LastRelative();
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

void TrackingSession::ResetFusionLocked() noexcept
{
    fusion_.Reset();
    hasLastOverlayVisual_ = false;
    lastOverlayVisual_ = {};
    hasFusedRelative_ = false;
    fusedRelative_ = core::Identity(core::Space::O, core::Space::O);
    lastFusion_ = {};
    lastFusion_.mode = fusionMode_;
}

void TrackingSession::FuseAndPublishLocked(
    std::chrono::steady_clock::time_point now,
    TrackingFrameMeta const* visualMeta,
    VisualEstimate const* overlayVisual)
{
    FusionContext context{};
    context.targetGeneration = policy_.TargetGeneration();
    context.geometryGeneration = policy_.GeometryGeneration();
    context.calibrationGeneration = policy_.CalibrationGeneration();
    context.nowMs = NowMs(now);

    FusionInputs inputs{};
    if (overlayVisual != nullptr && overlayVisual->reject == VisualReject::Ok && visualMeta != nullptr)
    {
        FusionSample visual = FromVisual(*overlayVisual);
        visual.targetGeneration = visualMeta->targetGeneration;
        visual.geometryGeneration = visualMeta->geometryGeneration;
        visual.calibrationGeneration = visualMeta->calibrationGeneration;
        visual.timestampMs = context.nowMs;
        inputs.hasVisual = true;
        inputs.visual = visual;
        lastOverlayVisual_ = visual;
        hasLastOverlayVisual_ = true;
    }

    if (fusionMode_ == FusionMode::Hybrid)
    {
        FusionPose const lastPose = fusion_.LastPose();
        FusionPose const* posePtr = fusion_.HasAcceptedPose() ? &lastPose : nullptr;

        FusionSample navigator = FromNavigator(navigatorSample_, fusionMapping_);
        StampSampleGenerations(navigator, context);
        if (navigator.timestampMs == 0)
        {
            navigator.timestampMs = context.nowMs;
        }
        inputs.hasNavigator = hasNavigatorSample_;
        inputs.navigator = navigator;

        FusionSample input = FromInput(inputSample_, posePtr);
        StampSampleGenerations(input, context);
        inputs.hasInput = hasInputSample_;
        inputs.input = input;

        FusionSample accessibility = FromAccessibility(accessibilitySample_, posePtr);
        StampSampleGenerations(accessibility, context);
        if (accessibility.timestampMs == 0)
        {
            accessibility.timestampMs = context.nowMs;
        }
        inputs.hasAccessibility = hasAccessibilitySample_;
        inputs.accessibility = accessibility;
    }

    FusionResult const fused = fusion_.Fuse(context, inputs);
    if (fused.accepted || visualMeta != nullptr)
    {
        lastFusion_ = fused;
    }

    if (fused.accepted)
    {
        bool const observerContributed =
            fused.usedNavigator || fused.usedInput || fused.usedAccessibility;
        VisualEstimate quality = overlayVisual != nullptr ? *overlayVisual : lastEstimate_;
        if (observerContributed)
        {
            VisualEstimate const published = EstimateFromFusion(fused, quality);
            fusedRelative_ = RelativeFromFusion(fused);
            hasFusedRelative_ = true;
            if (visualMeta != nullptr)
            {
                PublishLocked(visualMeta->captureTicks, visualMeta->sequence, published);
            }
            else
            {
                lastEstimate_ = published;
                snapshot_.confidence = published.confidence;
                snapshot_ = BuildSnapshotLocked(now);
            }
            return;
        }

        if (visualMeta != nullptr)
        {
            fusedRelative_ = policy_.LastRelative();
            hasFusedRelative_ = true;
            VisualEstimate published = quality;
            published.reject = VisualReject::Ok;
            if (overlayVisual != nullptr)
            {
                published = *overlayVisual;
                published.reject = VisualReject::Ok;
            }
            PublishLocked(visualMeta->captureTicks, visualMeta->sequence, published);
            return;
        }
    }

    snapshot_ = BuildSnapshotLocked(now);
}

} // namespace tracing::tracking
