#include <gtest/gtest.h>

#include "SequenceGenerator.h"
#include "core/Transform2D.h"
#include "tracking/TrackingSession.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace {

using tracing::core::Apply;
using tracing::core::MakeDocumentToScreen;
using tracing::core::MakeScreenToOverlay;
using tracing::core::Transform2D;
using tracing::core::Vec2;
using tracing::replay::BuildSequence;
using tracing::replay::kReplayDefaultSeed;
using tracing::replay::ReplayEvent;
using tracing::replay::ReplayEventName;
using tracing::replay::ReplayFrame;
using tracing::replay::ReplayLandmarks;
using tracing::tracking::FusionMode;
using tracing::tracking::FusionReject;
using tracing::tracking::kLostAge;
using tracing::tracking::NavigatorObservation;
using tracing::tracking::NavigatorReject;
using tracing::tracking::NavigatorSource;
using tracing::tracking::TrackingFrameAction;
using tracing::tracking::TrackingRoiFrame;
using tracing::tracking::TrackingSession;
using tracing::tracking::TrackingState;
using tracing::tracking::TransformSnapshot;
using tracing::tracking::VisualParityFromFlags;
using tracing::tracking::VisualReject;

using Clock = std::chrono::steady_clock;

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kMasterMedianPx = 2.0;
constexpr double kMasterP95Px = 5.0;
constexpr double kMasterScaleRel = 0.005;
constexpr double kMasterDegrees = 0.5;

bool BeginReplay(TrackingSession& session, std::string& error)
{
    Transform2D const mDs =
        MakeDocumentToScreen(Vec2{0.0, 0.0}, 0.0, 1.0, false, false, Vec2{0.0, 0.0});
    Transform2D const mSo = MakeScreenToOverlay(Vec2{0.0, 0.0});
    return session.BeginCalibrated(1, 2, 3, mDs, mSo, Vec2{0.0, 0.0}, error);
}

TrackingRoiFrame ToRoi(ReplayFrame const& frame)
{
    TrackingRoiFrame roi{};
    roi.meta.sequence = frame.sequence;
    roi.meta.captureTicks = static_cast<std::int64_t>(frame.sequence) * 1000;
    roi.meta.targetGeneration = 1;
    roi.meta.geometryGeneration = 2;
    roi.meta.calibrationGeneration = 3;
    roi.meta.width = frame.width;
    roi.meta.height = frame.height;
    roi.meta.stride = frame.stride;
    roi.meta.downsample = 1;
    roi.bgra = frame.bgra;
    return roi;
}

template <typename Pred>
bool WaitUntil(TrackingSession& session, Pred pred, int timeoutMs = 4000)
{
    auto const deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    do
    {
        if (pred(session.Snapshot()))
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (Clock::now() < deadline);
    return pred(session.Snapshot());
}

bool WaitKeyframe(TrackingSession& session, std::uint64_t sequence)
{
    return WaitUntil(session, [sequence](TransformSnapshot const& snap) {
        return snap.hasKeyframe && snap.sequence == sequence;
    });
}

bool WaitAccepted(TrackingSession& session, std::uint64_t sequence)
{
    return WaitUntil(session, [sequence](TransformSnapshot const& snap) {
        return snap.sequence == sequence && snap.lastReject == VisualReject::Ok &&
               (snap.state == TrackingState::Tracking || snap.state == TrackingState::Lost);
    });
}

std::size_t NearestRankIndex(double percentile, std::size_t count)
{
    if (count == 0)
    {
        return 0;
    }
    auto rank = static_cast<std::size_t>(std::ceil(percentile * static_cast<double>(count)));
    if (rank < 1)
    {
        rank = 1;
    }
    if (rank > count)
    {
        rank = count;
    }
    return rank - 1;
}

double WrapRadians(double radians)
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

struct LandmarkStats
{
    double median = 0.0;
    double p95 = 0.0;
    double max = 0.0;
    double scaleRel = 0.0;
    double degErr = 0.0;
    std::size_t count = 0;
};

LandmarkStats MeasureLandmarks(TransformSnapshot const& snap, ReplayFrame const& frame)
{
    LandmarkStats stats{};
    std::vector<Vec2> const landmarks = ReplayLandmarks();
    std::vector<double> errors;
    errors.reserve(landmarks.size());
    for (Vec2 const& src : landmarks)
    {
        std::optional<Vec2> const expected = Apply(frame.warp, src);
        std::optional<Vec2> const observed = Apply(snap.mDs, src);
        if (!expected.has_value() || !observed.has_value())
        {
            continue;
        }
        errors.push_back(std::hypot(observed->x - expected->x, observed->y - expected->y));
    }
    stats.count = errors.size();
    if (errors.empty())
    {
        return stats;
    }
    std::sort(errors.begin(), errors.end());
    stats.median = errors[NearestRankIndex(0.50, errors.size())];
    stats.p95 = errors[NearestRankIndex(0.95, errors.size())];
    stats.max = errors.back();

    double const scaleObs = std::hypot(snap.mDs.matrix.m[0], snap.mDs.matrix.m[1]);
    double const scaleGt = frame.uniformScale > 1.0e-12 ? frame.uniformScale : 1.0;
    stats.scaleRel = std::fabs(scaleObs / scaleGt - 1.0);
    double const angleObs = std::atan2(snap.mDs.matrix.m[1], snap.mDs.matrix.m[0]);
    stats.degErr = std::fabs(WrapRadians(angleObs - frame.radiansClockwise)) * 180.0 / kPi;
    return stats;
}

void PrintStats(char const* label, LandmarkStats const& stats, TransformSnapshot const& snap)
{
    std::cout << "SYNTHETIC replay " << label << " n=" << stats.count << " median=" << stats.median
              << " p95=" << stats.p95 << " max=" << stats.max << " scaleRel=" << stats.scaleRel
              << " degErr=" << stats.degErr << " state=" << static_cast<int>(snap.state)
              << " seq=" << snap.sequence << " conf=" << snap.confidence
              << " reject=" << static_cast<int>(snap.lastReject) << " flipX=" << snap.flipX
              << " (synthetic; not CSP)\n";
}

void ExpectMasterSettled(char const* label, LandmarkStats const& stats)
{
    EXPECT_GE(stats.count, 20u) << label;
    EXPECT_LE(stats.median, kMasterMedianPx) << label << " median=" << stats.median;
    EXPECT_LE(stats.p95, kMasterP95Px) << label << " p95=" << stats.p95;
    EXPECT_LE(stats.scaleRel, kMasterScaleRel) << label << " scaleRel=" << stats.scaleRel;
    EXPECT_LE(stats.degErr, kMasterDegrees) << label << " degErr=" << stats.degErr;
}

ReplayFrame const* FindEvent(std::vector<ReplayFrame> const& frames, ReplayEvent event)
{
    for (ReplayFrame const& frame : frames)
    {
        if (frame.event == event)
        {
            return &frame;
        }
    }
    return nullptr;
}

std::vector<ReplayFrame const*> FindEvents(
    std::vector<ReplayFrame> const& frames, ReplayEvent event)
{
    std::vector<ReplayFrame const*> matches;
    for (ReplayFrame const& frame : frames)
    {
        if (frame.event == event)
        {
            matches.push_back(&frame);
        }
    }
    return matches;
}

NavigatorObservation ObservedPan(double x, double y)
{
    NavigatorObservation observation{};
    observation.source = NavigatorSource::Observed;
    observation.reject = NavigatorReject::Ok;
    observation.hasTranslation = true;
    observation.hasDocumentPosition = true;
    observation.documentX = x;
    observation.documentY = y;
    observation.confidence = 0.8;
    observation.targetGeneration = 1;
    observation.geometryGeneration = 2;
    observation.sequence = 2;
    return observation;
}

NavigatorObservation MissingNavigator()
{
    NavigatorObservation observation{};
    observation.source = NavigatorSource::Missing;
    observation.reject = NavigatorReject::BlankOrNoIndicator;
    return observation;
}

tracing::platform::InputObservation PredictedPan(
    double dx, double dy, std::int64_t ticksMs, std::uint64_t generation)
{
    tracing::platform::InputObservation observation{};
    observation.source = tracing::platform::InputSource::Predicted;
    observation.reject = tracing::platform::InputReject::Ok;
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

tracing::platform::AccessibilitySnapshot UnsupportedUia()
{
    tracing::platform::AccessibilitySnapshot snapshot{};
    snapshot.capability = tracing::platform::AccessibilityCapability::Unsupported;
    snapshot.reject = tracing::platform::AccessibilityReject::Unsupported;
    snapshot.enabled = false;
    snapshot.targetGeneration = 1;
    return snapshot;
}

std::int64_t NowMs(Clock::time_point now)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

} // namespace

TEST(TrackingReplay, SequenceMetadataIsSeededAndIndependentOfEstimator)
{
    std::vector<Vec2> const landmarks = ReplayLandmarks();
    EXPECT_GE(landmarks.size(), 20u);

    std::vector<ReplayFrame> const frames = BuildSequence(kReplayDefaultSeed);
    EXPECT_EQ(BuildSequence(kReplayDefaultSeed).size(), frames.size());
    ASSERT_FALSE(frames.empty());
    EXPECT_EQ(frames.front().event, ReplayEvent::Keyframe);
    EXPECT_EQ(frames.front().sequence, 1u);

    bool sawMissing = false;
    bool sawBlank = false;
    bool sawStroke = false;
    int flipCount = 0;
    int reacquireCount = 0;
    int settled = 0;
    for (ReplayFrame const& frame : frames)
    {
        if (frame.missing)
        {
            sawMissing = true;
            EXPECT_TRUE(frame.bgra.empty());
            EXPECT_EQ(frame.event, ReplayEvent::Missing);
        }
        if (frame.blank)
        {
            sawBlank = true;
            EXPECT_FALSE(frame.bgra.empty());
        }
        if (frame.stroke)
        {
            sawStroke = true;
        }
        if (frame.event == ReplayEvent::FlipX)
        {
            ++flipCount;
            EXPECT_TRUE(frame.flipX);
        }
        if (frame.event == ReplayEvent::Reacquire)
        {
            ++reacquireCount;
        }
        if (frame.settledMaster)
        {
            ++settled;
            EXPECT_FALSE(frame.missing);
            EXPECT_FALSE(frame.blank);
        }
    }
    EXPECT_TRUE(sawMissing);
    EXPECT_TRUE(sawBlank);
    EXPECT_TRUE(sawStroke);
    EXPECT_EQ(flipCount, 4);
    EXPECT_EQ(reacquireCount, 3);
    EXPECT_GE(settled, 5);
}

TEST(TrackingReplay, SettledTexturedFramesMeetMasterPixelTargets)
{
    // Synthetic 320x240 ROI pixels. Not 1080p CSP and not an OBS result.
    // Each settled warp is measured as an isolated keyframe hop. Feeding scale/rotate
    // after pan in one session still compares those images to the origin keyframe,
    // and that later hop can be parity-ambiguous.
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ASSERT_NE(key, nullptr);

    int measured = 0;
    for (ReplayFrame const& frame : frames)
    {
        if (!frame.settledMaster || frame.event == ReplayEvent::GapPan)
        {
            continue;
        }
        TrackingSession session;
        std::string error;
        ASSERT_TRUE(BeginReplay(session, error)) << error;
        ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
        ASSERT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
        ASSERT_EQ(session.SubmitRoiFrame(ToRoi(frame)), TrackingFrameAction::Enqueue)
            << ReplayEventName(frame.event);
        bool const accepted = WaitAccepted(session, frame.sequence);
        TransformSnapshot const snap = session.Snapshot();
        LandmarkStats const stats = MeasureLandmarks(snap, frame);
        PrintStats(ReplayEventName(frame.event), stats, snap);
        if (!accepted || snap.state != TrackingState::Tracking)
        {
            std::cout << "FAILING SCENARIO " << ReplayEventName(frame.event) << " "
                      << session.FormatReport() << " (synthetic; MASTER bounds not asserted)\n";
            EXPECT_TRUE(snap.lastReject == VisualReject::ParityAmbiguous ||
                        snap.state == TrackingState::Paused ||
                        snap.state == TrackingState::Degraded || snap.state == TrackingState::Lost)
                << ReplayEventName(frame.event);
            session.Stop();
            continue;
        }
        EXPECT_FALSE(snap.flipX) << ReplayEventName(frame.event);
        EXPECT_FALSE(snap.flipY) << ReplayEventName(frame.event);
        ExpectMasterSettled(ReplayEventName(frame.event), stats);
        ++measured;
        session.Stop();
    }
    EXPECT_GE(measured, 3);
}

TEST(TrackingReplay, CombinedFromKeyframeIsRecordedSeparately)
{
    // Isolated hop. Sequential MASTER skips Combined because a later origin-keyframe
    // estimate can be parity-ambiguous after other poses.
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ReplayFrame const* combined = FindEvent(frames, ReplayEvent::Combined);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(combined, nullptr);

    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginReplay(session, error)) << error;
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*combined)), TrackingFrameAction::Enqueue);
    bool const accepted = WaitAccepted(session, combined->sequence);
    TransformSnapshot const snap = session.Snapshot();
    LandmarkStats const stats = MeasureLandmarks(snap, *combined);
    PrintStats("combined-from-keyframe", stats, snap);
    if (!accepted || snap.state != TrackingState::Tracking ||
        snap.lastReject == VisualReject::ParityAmbiguous)
    {
        std::cout << "FAILING SCENARIO combined-from-keyframe " << session.FormatReport()
                  << " (synthetic; MASTER bounds not asserted)\n";
        EXPECT_TRUE(snap.state == TrackingState::Paused || snap.state == TrackingState::Degraded ||
                    snap.state == TrackingState::Lost ||
                    snap.lastReject == VisualReject::ParityAmbiguous);
        session.Stop();
        return;
    }
    EXPECT_FALSE(snap.flipX);
    EXPECT_FALSE(snap.flipY);
    ExpectMasterSettled("combined-from-keyframe", stats);
    session.Stop();
}

TEST(TrackingReplay, MissingFrameGapDoesNotJumpThenBlankLosesConfidence)
{
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ReplayFrame const* pan = FindEvent(frames, ReplayEvent::Pan);
    ReplayFrame const* missing = FindEvent(frames, ReplayEvent::Missing);
    ReplayFrame const* gap = FindEvent(frames, ReplayEvent::GapPan);
    ReplayFrame const* blank = FindEvent(frames, ReplayEvent::Blank);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(pan, nullptr);
    ASSERT_NE(missing, nullptr);
    ASSERT_NE(gap, nullptr);
    ASSERT_NE(blank, nullptr);

    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginReplay(session, error)) << error;
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*pan)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitAccepted(session, pan->sequence)) << session.FormatReport();

    TransformSnapshot const afterPan = session.Snapshot();
    std::optional<Vec2> const originPan = Apply(afterPan.mDs, Vec2{});
    ASSERT_TRUE(originPan.has_value());
    EXPECT_NEAR(originPan->x, 8.0, kMasterP95Px);
    EXPECT_NEAR(originPan->y, 5.0, kMasterP95Px);

    EXPECT_TRUE(missing->missing);
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*gap)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitAccepted(session, gap->sequence)) << session.FormatReport();
    TransformSnapshot const afterGap = session.Snapshot();
    EXPECT_EQ(afterGap.sequence, gap->sequence);
    EXPECT_NE(afterGap.sequence, missing->sequence);
    LandmarkStats const gapStats = MeasureLandmarks(afterGap, *gap);
    PrintStats("gap-pan", gapStats, afterGap);
    ExpectMasterSettled("gap-pan", gapStats);

    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*blank)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.lastReject == VisualReject::BlankOrLowTexture &&
               (snap.state == TrackingState::Degraded || snap.state == TrackingState::Lost);
    })) << session.FormatReport();

    TransformSnapshot const afterBlank = session.Snapshot();
    EXPECT_EQ(afterBlank.sequence, gap->sequence);
    EXPECT_EQ(afterBlank.lastReject, VisualReject::BlankOrLowTexture);
    std::cout << "SYNTHETIC replay blank conf=" << afterBlank.confidence
              << " state=" << static_cast<int>(afterBlank.state)
              << " (last accepted confidence may remain; blank was not Ok)\n";
    EXPECT_FALSE(afterBlank.flipX);
    EXPECT_FALSE(afterBlank.flipY);
    std::optional<Vec2> const originBlank = Apply(afterBlank.mDs, Vec2{});
    ASSERT_TRUE(originBlank.has_value());
    EXPECT_NEAR(originBlank->x, originPan->x, 1.0e-6);
    EXPECT_NEAR(originBlank->y, originPan->y, 1.0e-6);
    session.Stop();
}

TEST(TrackingReplay, LostThenReacquireWithinOneSecond)
{
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ReplayFrame const* pan = FindEvent(frames, ReplayEvent::Pan);
    ReplayFrame const* blank = FindEvent(frames, ReplayEvent::Blank);
    std::vector<ReplayFrame const*> reacquire = FindEvents(frames, ReplayEvent::Reacquire);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(pan, nullptr);
    ASSERT_NE(blank, nullptr);
    ASSERT_EQ(reacquire.size(), 3u);

    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginReplay(session, error)) << error;
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*pan)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitAccepted(session, pan->sequence)) << session.FormatReport();
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*blank)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.lastReject == VisualReject::BlankOrLowTexture &&
               (snap.state == TrackingState::Degraded || snap.state == TrackingState::Lost);
    })) << session.FormatReport();

    if (session.Snapshot().state != TrackingState::Lost)
    {
        session.Tick(Clock::now() + kLostAge);
    }
    TransformSnapshot const lost = session.Snapshot();
    EXPECT_EQ(lost.state, TrackingState::Lost);
    EXPECT_TRUE(lost.hideOverlay);
    EXPECT_FALSE(lost.flipX);

    auto const t0 = Clock::now();
    for (std::size_t i = 0; i < reacquire.size(); ++i)
    {
        ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*reacquire[i])), TrackingFrameAction::Enqueue);
        if (i + 1 < reacquire.size())
        {
            ASSERT_TRUE(WaitUntil(session, [seq = reacquire[i]->sequence](TransformSnapshot const& snap) {
                return snap.sequence == seq && snap.lastReject == VisualReject::Ok;
            })) << session.FormatReport();
            EXPECT_EQ(session.Snapshot().state, TrackingState::Lost);
        }
        else
        {
            ASSERT_TRUE(WaitUntil(session, [seq = reacquire[i]->sequence](TransformSnapshot const& snap) {
                return snap.sequence == seq && snap.state == TrackingState::Tracking &&
                       snap.reacquireCount >= 1;
            })) << session.FormatReport();
        }
    }
    auto const elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    TransformSnapshot const after = session.Snapshot();
    std::cout << "SYNTHETIC replay reacquireMs=" << elapsedMs
              << " reacquireCount=" << after.reacquireCount << " state=" << static_cast<int>(after.state)
              << " (synthetic; not CSP)\n";
    EXPECT_EQ(after.state, TrackingState::Tracking);
    EXPECT_FALSE(after.hideOverlay);
    EXPECT_GE(after.reacquireCount, 1);
    if (elapsedMs > 1000)
    {
        std::cout << "MASTER recovery <=1s NOT MET on synthetic worker path elapsedMs=" << elapsedMs
                  << " (3 reacquire frames, each may run multiple ORB fits)\n";
    }
    LandmarkStats const stats = MeasureLandmarks(after, *reacquire.back());
    PrintStats("reacquire", stats, after);
    session.Stop();
}

TEST(TrackingReplay, FlipXHysteresisThenNoConfidentWrongParity)
{
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ReplayFrame const* pan = FindEvent(frames, ReplayEvent::Pan);
    std::vector<ReplayFrame const*> flips = FindEvents(frames, ReplayEvent::FlipX);
    std::vector<ReplayFrame const*> identity = FindEvents(frames, ReplayEvent::Identity);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(pan, nullptr);
    ASSERT_EQ(flips.size(), 4u);
    ASSERT_EQ(identity.size(), 2u);

    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginReplay(session, error)) << error;
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*pan)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitAccepted(session, pan->sequence)) << session.FormatReport();
    TransformSnapshot const before = session.Snapshot();
    std::optional<Vec2> const originBefore = Apply(before.mDs, Vec2{});
    ASSERT_TRUE(originBefore.has_value());
    EXPECT_FALSE(before.flipX);

    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*flips[0])), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [panSeq = pan->sequence](TransformSnapshot const& snap) {
        return snap.sequence == panSeq && !snap.flipX &&
               (snap.state == TrackingState::Degraded || snap.state == TrackingState::Lost ||
                snap.state == TrackingState::Paused);
    })) << session.FormatReport();
    TransformSnapshot const held = session.Snapshot();
    EXPECT_EQ(held.sequence, pan->sequence);
    EXPECT_FALSE(held.flipX);
    EXPECT_EQ(VisualParityFromFlags(held.flipX, held.flipY), tracing::tracking::VisualParity::None);
    std::optional<Vec2> const originHeld = Apply(held.mDs, Vec2{});
    ASSERT_TRUE(originHeld.has_value());
    EXPECT_NEAR(originHeld->x, originBefore->x, 1.0e-6);
    EXPECT_NEAR(originHeld->y, originBefore->y, 1.0e-6);

    for (std::size_t i = 1; i < flips.size(); ++i)
    {
        ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*flips[i])), TrackingFrameAction::Enqueue);
        ASSERT_TRUE(WaitUntil(session, [seq = flips[i]->sequence](TransformSnapshot const& snap) {
            return snap.sequence == seq && snap.lastReject == VisualReject::Ok;
        })) << "flip-x[" << i << "] " << session.FormatReport();
    }
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.flipX && snap.lastReject == VisualReject::Ok;
    })) << session.FormatReport();
    TransformSnapshot const flipped = session.Snapshot();
    EXPECT_TRUE(flipped.flipX);
    EXPECT_FALSE(flipped.flipY);
    EXPECT_EQ(VisualParityFromFlags(flipped.flipX, flipped.flipY), tracing::tracking::VisualParity::FlipX);
    LandmarkStats const flipStats = MeasureLandmarks(flipped, *flips.back());
    PrintStats("flip-x", flipStats, flipped);

    if (flipped.state != TrackingState::Tracking)
    {
        std::cout << "SYNTHETIC replay flip-x ended state=" << static_cast<int>(flipped.state)
                  << " (ORB age may keep Lost; parity published without confident-wrong first hop)\n";
        session.Stop();
        return;
    }

    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*identity[0])), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [flipSeq = flips.back()->sequence](TransformSnapshot const& snap) {
        return snap.sequence == flipSeq && snap.flipX &&
               (snap.state == TrackingState::Degraded || snap.state == TrackingState::Lost);
    })) << session.FormatReport();
    TransformSnapshot const identityHeld = session.Snapshot();
    EXPECT_TRUE(identityHeld.flipX);

    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*identity[1])), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [seq = identity[1]->sequence](TransformSnapshot const& snap) {
        return snap.sequence == seq && snap.lastReject == VisualReject::Ok;
    })) << session.FormatReport();
    TransformSnapshot const restored = session.Snapshot();
    if (restored.state == TrackingState::Tracking)
    {
        EXPECT_FALSE(restored.flipX);
        EXPECT_FALSE(restored.flipY);
        EXPECT_EQ(
            VisualParityFromFlags(restored.flipX, restored.flipY), tracing::tracking::VisualParity::None);
    }
    session.Stop();
}

TEST(TrackingReplay, VisualOnlyIgnoresObserversOnGeneratedPan)
{
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ReplayFrame const* pan = FindEvent(frames, ReplayEvent::Pan);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(pan, nullptr);

    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginReplay(session, error)) << error;
    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
    TransformSnapshot const before = session.Snapshot();
    std::optional<Vec2> const originBefore = Apply(before.mDs, Vec2{});
    ASSERT_TRUE(originBefore.has_value());
    EXPECT_EQ(session.GetFusionMode(), FusionMode::VisualOnly);

    auto const t0 = Clock::now();
    session.SetObserverSamples(
        ObservedPan(8.0, 5.0), PredictedPan(10.0, 0.0, NowMs(t0), 1), UnsupportedUia(), {});
    session.Tick(t0);
    TransformSnapshot const ignored = session.Snapshot();
    std::optional<Vec2> const originIgnored = Apply(ignored.mDs, Vec2{});
    ASSERT_TRUE(originIgnored.has_value());
    EXPECT_NEAR(originIgnored->x, originBefore->x, 1.0e-9);
    EXPECT_NEAR(originIgnored->y, originBefore->y, 1.0e-9);
    EXPECT_FALSE(ignored.usedInput);
    EXPECT_FALSE(ignored.usedNavigator);

    ASSERT_EQ(session.SubmitRoiFrame(ToRoi(*pan)), TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitAccepted(session, pan->sequence)) << session.FormatReport();
    TransformSnapshot const after = session.Snapshot();
    LandmarkStats const stats = MeasureLandmarks(after, *pan);
    PrintStats("visual-only-pan", stats, after);
    ExpectMasterSettled("visual-only-pan", stats);
    EXPECT_TRUE(after.usedVisual);
    EXPECT_FALSE(after.usedInput);
    session.Stop();
}

TEST(TrackingReplay, HybridContradictionDoesNotIncreaseConfidentWrong)
{
    std::vector<ReplayFrame> const frames = BuildSequence();
    ReplayFrame const* key = FindEvent(frames, ReplayEvent::Keyframe);
    ReplayFrame const* pan = FindEvent(frames, ReplayEvent::Pan);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(pan, nullptr);

    auto run = [&](bool hybrid, bool contradict) {
        TrackingSession session;
        std::string error;
        EXPECT_TRUE(BeginReplay(session, error)) << error;
        EXPECT_EQ(session.SubmitRoiFrame(ToRoi(*key)), TrackingFrameAction::Enqueue);
        EXPECT_TRUE(WaitKeyframe(session, key->sequence)) << session.FormatReport();
        if (hybrid)
        {
            session.SetFusionMode(FusionMode::Hybrid);
        }
        NavigatorObservation const nav = contradict ? ObservedPan(80.0, 0.0) : ObservedPan(8.0, 5.0);
        session.SetObserverSamples(nav, tracing::platform::InputObservation{}, UnsupportedUia(), {});
        EXPECT_EQ(session.SubmitRoiFrame(ToRoi(*pan)), TrackingFrameAction::Enqueue);
        EXPECT_TRUE(WaitAccepted(session, pan->sequence)) << session.FormatReport();
        TransformSnapshot const after = session.Snapshot();
        std::optional<Vec2> const origin = Apply(after.mDs, Vec2{});
        int confidentWrong = 0;
        if (origin.has_value() && after.confidence >= 0.5 &&
            std::hypot(origin->x - 8.0, origin->y - 5.0) > 12.0)
        {
            confidentWrong = 1;
        }
        session.Stop();
        return std::tuple<int, bool, double, double, FusionReject>(
            confidentWrong,
            after.usedVisual,
            origin.has_value() ? origin->x : 0.0,
            origin.has_value() ? origin->y : 0.0,
            after.fusionReject);
    };

    auto const visualAgree = run(false, false);
    auto const hybridAgree = run(true, false);
    auto const visualContra = run(false, true);
    auto const hybridContra = run(true, true);

    std::cout << "SYNTHETIC replay fusion visualAgree wrong=" << std::get<0>(visualAgree)
              << " tx=" << std::get<2>(visualAgree) << " hybridAgree wrong=" << std::get<0>(hybridAgree)
              << " tx=" << std::get<2>(hybridAgree) << " visualContra wrong=" << std::get<0>(visualContra)
              << " tx=" << std::get<2>(visualContra) << " hybridContra wrong=" << std::get<0>(hybridContra)
              << " tx=" << std::get<2>(hybridContra) << " (synthetic; not CSP)\n";

    EXPECT_EQ(std::get<0>(visualAgree), 0);
    EXPECT_EQ(std::get<0>(hybridAgree), 0);
    EXPECT_NEAR(std::get<2>(visualAgree), 8.0, kMasterP95Px);
    EXPECT_NEAR(std::get<3>(visualAgree), 5.0, kMasterP95Px);
    EXPECT_NEAR(std::get<2>(hybridAgree), 8.0, kMasterP95Px);
    EXPECT_NEAR(std::get<3>(hybridAgree), 5.0, kMasterP95Px);
    EXPECT_EQ(std::get<0>(hybridContra), 0);
    EXPECT_LE(std::get<0>(hybridContra), std::get<0>(visualContra));
    EXPECT_NEAR(std::get<2>(hybridContra), 8.0, kMasterP95Px);
    EXPECT_NEAR(std::get<3>(hybridContra), 5.0, kMasterP95Px);
    EXPECT_TRUE(std::get<1>(hybridContra));
}
