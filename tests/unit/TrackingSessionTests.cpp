#include <gtest/gtest.h>

#include "core/Transform2D.h"
#include "tracking/TrackingSession.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

using tracing::core::Apply;
using tracing::core::MakeDocumentToScreen;
using tracing::core::MakeScreenToOverlay;
using tracing::core::Transform2D;
using tracing::core::Vec2;
using tracing::tracking::FormatTrackingFrameAction;
using tracing::tracking::FormatTrackingState;
using tracing::tracking::kDegradedAge;
using tracing::tracking::kLostAge;
using tracing::tracking::SimilarityFromEstimate;
using tracing::tracking::TrackingApplyAction;
using tracing::tracking::TrackingFrameAction;
using tracing::tracking::TrackingFrameMeta;
using tracing::tracking::TrackingHandoffResult;
using tracing::tracking::TrackingRoiFrame;
using tracing::tracking::TrackingSession;
using tracing::tracking::TrackingSessionPolicy;
using tracing::tracking::TrackingState;
using tracing::tracking::TransformSnapshot;
using tracing::tracking::VisualEstimate;
using tracing::tracking::VisualReject;

using Clock = std::chrono::steady_clock;

TrackingFrameMeta Meta(
    std::uint64_t sequence,
    std::uint64_t target = 1,
    std::uint64_t geometry = 2,
    std::uint64_t calibration = 3)
{
    TrackingFrameMeta meta{};
    meta.sequence = sequence;
    meta.captureTicks = static_cast<std::int64_t>(sequence) * 1000;
    meta.targetGeneration = target;
    meta.geometryGeneration = geometry;
    meta.calibrationGeneration = calibration;
    meta.width = 32;
    meta.height = 32;
    meta.stride = 32 * 4;
    meta.downsample = 1;
    return meta;
}

VisualEstimate OkEstimate(double tx = 0.0, double ty = 0.0, double scale = 1.0, double radians = 0.0)
{
    VisualEstimate estimate{};
    estimate.reject = VisualReject::Ok;
    estimate.tx = tx;
    estimate.ty = ty;
    estimate.uniformScale = scale;
    estimate.radiansClockwise = radians;
    estimate.confidence = 0.5;
    estimate.inlierCount = 20;
    estimate.matchCount = 24;
    estimate.rmsResidualPx = 0.4;
    return estimate;
}

VisualEstimate BlankEstimate()
{
    VisualEstimate estimate{};
    estimate.reject = VisualReject::BlankOrLowTexture;
    estimate.confidence = 0.0;
    return estimate;
}

bool BeginIdentity(TrackingSession& session, std::string& error)
{
    Transform2D const mDs =
        MakeDocumentToScreen(Vec2{100.0, 200.0}, 0.0, 2.0, false, false, Vec2{0.0, 0.0});
    Transform2D const mSo = MakeScreenToOverlay(Vec2{100.0, 200.0});
    return session.BeginCalibrated(1, 2, 3, mDs, mSo, Vec2{100.0, 200.0}, error);
}

constexpr int kWorkerWidth = 320;
constexpr int kWorkerHeight = 240;

cv::Mat MakeTexturedGray(std::uint32_t seed)
{
    cv::Mat gray(kWorkerHeight, kWorkerWidth, CV_8UC1);
    std::uint32_t rng = seed;
    auto next = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return rng;
    };
    for (int y = 0; y < kWorkerHeight; ++y)
    {
        for (int x = 0; x < kWorkerWidth; ++x)
        {
            std::uint32_t const noise = next() >> 24;
            int const checker = ((x / 10) ^ (y / 10)) & 1;
            int const value = static_cast<int>(noise) + checker * 70 + ((x * 17 + y * 11) & 47);
            gray.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(std::clamp(value, 0, 255));
        }
    }
    cv::circle(gray, cv::Point(48, 42), 18, cv::Scalar(255), cv::FILLED);
    cv::circle(gray, cv::Point(kWorkerWidth - 56, 46), 14, cv::Scalar(16), cv::FILLED);
    cv::rectangle(gray, cv::Point(28, kWorkerHeight - 72), cv::Point(96, kWorkerHeight - 22),
                  cv::Scalar(230), cv::FILLED);
    cv::rectangle(gray, cv::Point(150, 96), cv::Point(214, 110), cv::Scalar(8), cv::FILLED);
    cv::rectangle(gray, cv::Point(150, 96), cv::Point(164, 176), cv::Scalar(8), cv::FILLED);
    cv::line(gray, cv::Point(18, 18), cv::Point(kWorkerWidth - 24, 78), cv::Scalar(250), 3);
    cv::line(gray, cv::Point(40, kWorkerHeight - 18), cv::Point(kWorkerWidth - 30, kWorkerHeight - 90),
             cv::Scalar(12), 3);
    return gray;
}

cv::Mat GrayToBgra(cv::Mat const& gray)
{
    cv::Mat bgra;
    cv::cvtColor(gray, bgra, cv::COLOR_GRAY2BGRA);
    return bgra;
}

TrackingRoiFrame FrameFromBgra(cv::Mat const& bgra, std::uint64_t sequence)
{
    TrackingRoiFrame frame{};
    frame.meta = Meta(sequence);
    frame.meta.width = bgra.cols;
    frame.meta.height = bgra.rows;
    frame.meta.stride = bgra.cols * 4;
    std::size_t const bytes =
        static_cast<std::size_t>(frame.meta.stride) * static_cast<std::size_t>(frame.meta.height);
    frame.bgra.resize(bytes);
    if (bgra.isContinuous() && bgra.elemSize() == 4)
    {
        std::memcpy(frame.bgra.data(), bgra.data, bytes);
    }
    else
    {
        for (int y = 0; y < bgra.rows; ++y)
        {
            std::memcpy(
                frame.bgra.data() + static_cast<std::size_t>(y * frame.meta.stride),
                bgra.ptr(y),
                static_cast<std::size_t>(bgra.cols * 4));
        }
    }
    return frame;
}

template <typename Pred>
bool WaitUntil(TrackingSession& session, Pred pred, int timeoutMs = 2500)
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

} // namespace

TEST(TrackingPolicy, WrongGenerationRejected)
{
    TrackingSessionPolicy policy;
    policy.BeginCalibrated(1, 2, 3);
    TrackingHandoffResult const ok = policy.Arrive(Meta(1, 1, 2, 3));
    EXPECT_EQ(ok.action, TrackingFrameAction::Enqueue);
    TrackingHandoffResult const bad = policy.Arrive(Meta(2, 9, 2, 3));
    EXPECT_EQ(bad.action, TrackingFrameAction::RejectWrongGeneration);
    EXPECT_STREQ(FormatTrackingFrameAction(bad.action), "reject-wrong-generation");
}

TEST(TrackingPolicy, OutOfOrderRejectedAfterAccept)
{
    TrackingSessionPolicy policy;
    policy.BeginCalibrated(1, 2, 3);
    auto const t0 = Clock::now();
    EXPECT_EQ(policy.Arrive(Meta(1)).action, TrackingFrameAction::Enqueue);
    policy.NoteDequeued();
    policy.NoteKeyframeCaptured(t0, 1);
    EXPECT_EQ(policy.LastAcceptedSequence(), 1u);
    EXPECT_EQ(policy.Arrive(Meta(1)).action, TrackingFrameAction::RejectOutOfOrder);
}

TEST(TrackingSession, AgeEntersDegradedThenLostAndHides)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);

    TransformSnapshot snap = session.Snapshot();
    EXPECT_EQ(snap.state, TrackingState::Calibrating);
    EXPECT_FALSE(snap.hideOverlay);
    EXPECT_TRUE(snap.hasKeyframe);

    session.Tick(t0 + kDegradedAge);
    snap = session.Snapshot();
    EXPECT_EQ(snap.state, TrackingState::Degraded);
    EXPECT_FALSE(snap.hideOverlay);
    EXPECT_GE(snap.observationAgeMs, kDegradedAge.count());

    session.Tick(t0 + kLostAge);
    snap = session.Snapshot();
    EXPECT_EQ(snap.state, TrackingState::Lost);
    EXPECT_TRUE(snap.hideOverlay);
    EXPECT_STREQ(FormatTrackingState(snap.state), "Lost");
}

TEST(TrackingSession, OutOfOrderEstimateLeavesSnapshotUnchanged)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);
    auto const accepted = session.SubmitEstimateForTest(Meta(5), OkEstimate(4.0, 0.0), false, {}, t0);
    EXPECT_EQ(accepted.action, TrackingApplyAction::Accepted);
    TransformSnapshot const afterOk = session.Snapshot();
    EXPECT_EQ(afterOk.sequence, 5u);
    EXPECT_EQ(afterOk.state, TrackingState::Tracking);

    auto const ignored = session.SubmitEstimateForTest(Meta(4), OkEstimate(40.0, 0.0), false, {}, t0);
    EXPECT_EQ(ignored.action, TrackingApplyAction::Ignored);
    TransformSnapshot const afterOld = session.Snapshot();
    EXPECT_EQ(afterOld.sequence, 5u);
    std::optional<Vec2> const before = Apply(afterOk.mDs, Vec2{0.0, 0.0});
    std::optional<Vec2> const after = Apply(afterOld.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(before.has_value());
    ASSERT_TRUE(after.has_value());
    EXPECT_NEAR(before->x, after->x, 1.0e-9);
    EXPECT_NEAR(before->y, after->y, 1.0e-9);
}

TEST(TrackingSession, WrongGenerationEstimateDoesNotMoveTransform)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);
    session.SubmitEstimateForTest(Meta(2), OkEstimate(), false, {}, t0);
    TransformSnapshot const tracking = session.Snapshot();
    EXPECT_EQ(tracking.state, TrackingState::Tracking);

    auto const ignored =
        session.SubmitEstimateForTest(Meta(3, 99, 2, 3), OkEstimate(80.0, 0.0), false, {}, t0);
    EXPECT_EQ(ignored.action, TrackingApplyAction::Ignored);
    TransformSnapshot const after = session.Snapshot();
    EXPECT_EQ(after.sequence, 2u);
    EXPECT_EQ(after.state, TrackingState::Tracking);
}

TEST(TrackingSession, ContradictoryPreviousEstimateRejected)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);
    TransformSnapshot const key = session.Snapshot();
    std::optional<Vec2> const originBefore = Apply(key.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(originBefore.has_value());

    VisualEstimate const keyframeOk = OkEstimate(0.0, 0.0);
    VisualEstimate const previousJump = OkEstimate(80.0, 0.0);
    auto const result =
        session.SubmitEstimateForTest(Meta(2), keyframeOk, true, previousJump, t0);
    EXPECT_EQ(result.action, TrackingApplyAction::RejectedContradiction);
    TransformSnapshot const after = session.Snapshot();
    EXPECT_EQ(after.state, TrackingState::Degraded);
    EXPECT_EQ(after.sequence, 1u);
    std::optional<Vec2> const originAfter = Apply(after.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(originAfter.has_value());
    EXPECT_NEAR(originBefore->x, originAfter->x, 1.0e-9);
    EXPECT_NEAR(originBefore->y, originAfter->y, 1.0e-9);
}

TEST(TrackingSession, BlankDoesNotPublishConfidentTransform)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);
    auto const result = session.SubmitEstimateForTest(Meta(2), BlankEstimate(), false, {}, t0);
    EXPECT_EQ(result.action, TrackingApplyAction::RejectedQuality);
    TransformSnapshot const snap = session.Snapshot();
    EXPECT_EQ(snap.state, TrackingState::Degraded);
    EXPECT_EQ(snap.lastReject, VisualReject::BlankOrLowTexture);
    EXPECT_EQ(snap.sequence, 1u);
    EXPECT_DOUBLE_EQ(snap.confidence, 1.0);
}

TEST(TrackingSession, ThreeConsistentEstimatesReacquireFromLost)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);
    session.Tick(t0 + kLostAge);
    EXPECT_EQ(session.Snapshot().state, TrackingState::Lost);
    EXPECT_TRUE(session.Snapshot().hideOverlay);

    auto const t1 = t0 + kLostAge + std::chrono::milliseconds(10);
    EXPECT_EQ(
        session.SubmitEstimateForTest(Meta(2), OkEstimate(), false, {}, t1).action,
        TrackingApplyAction::Accepted);
    EXPECT_EQ(session.Snapshot().state, TrackingState::Lost);
    EXPECT_TRUE(session.Snapshot().hideOverlay);

    EXPECT_EQ(
        session.SubmitEstimateForTest(Meta(3), OkEstimate(), false, {}, t1).action,
        TrackingApplyAction::Accepted);
    EXPECT_EQ(session.Snapshot().state, TrackingState::Lost);

    EXPECT_EQ(
        session.SubmitEstimateForTest(Meta(4), OkEstimate(), false, {}, t1).action,
        TrackingApplyAction::Reacquired);
    TransformSnapshot const snap = session.Snapshot();
    EXPECT_EQ(snap.state, TrackingState::Tracking);
    EXPECT_FALSE(snap.hideOverlay);
    EXPECT_EQ(snap.sequence, 4u);
}

TEST(TrackingSession, WindowMoveUpdatesOriginWithoutChangingRelative)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;
    auto const t0 = Clock::now();
    session.CaptureKeyframeForTest(Meta(1), t0);
    session.SubmitEstimateForTest(Meta(2), OkEstimate(5.0, 0.0), false, {}, t0);
    TransformSnapshot const before = session.Snapshot();
    std::optional<Vec2> const pBefore = Apply(before.mDs, Vec2{10.0, 5.0});
    ASSERT_TRUE(pBefore.has_value());

    session.SetViewportAnchorS(Vec2{140.0, 240.0});
    TransformSnapshot const after = session.Snapshot();
    std::optional<Vec2> const pAfter = Apply(after.mDs, Vec2{10.0, 5.0});
    ASSERT_TRUE(pAfter.has_value());
    EXPECT_NEAR(pAfter->x - pBefore->x, 40.0, 1.0e-6);
    EXPECT_NEAR(pAfter->y - pBefore->y, 40.0, 1.0e-6);
}

TEST(TrackingSession, StopFromSubmitThreadDoesNotDeadlock)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;

    TrackingRoiFrame frame{};
    frame.meta = Meta(1);
    frame.bgra.assign(static_cast<std::size_t>(frame.meta.stride * frame.meta.height), 80);
    TrackingFrameAction const action = session.SubmitRoiFrame(std::move(frame));
    EXPECT_TRUE(
        action == TrackingFrameAction::Enqueue ||
        action == TrackingFrameAction::DropOldestThenEnqueue);

    session.Stop();
    TransformSnapshot const snap = session.Snapshot();
    EXPECT_EQ(snap.state, TrackingState::Unattached);
    EXPECT_FALSE(session.Snapshot().hasKeyframe);

    session.Stop();
}

TEST(TrackingSession, WorkerTexturedTranslationMovesSnapshot)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;

    cv::Mat const prior = MakeTexturedGray(0xB6AAu);
    cv::Mat affine = cv::Mat::eye(2, 3, CV_64F);
    affine.at<double>(0, 2) = 8.0;
    affine.at<double>(1, 2) = 5.0;
    cv::Mat currentGray;
    cv::warpAffine(
        prior,
        currentGray,
        affine,
        prior.size(),
        cv::INTER_LINEAR,
        cv::BORDER_CONSTANT,
        cv::Scalar(128));

    TrackingFrameAction const keyAction =
        session.SubmitRoiFrame(FrameFromBgra(GrayToBgra(prior), 1));
    EXPECT_EQ(keyAction, TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.hasKeyframe && snap.sequence == 1u;
    })) << session.FormatReport();

    TransformSnapshot const key = session.Snapshot();
    std::optional<Vec2> const originBefore = Apply(key.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(originBefore.has_value());

    TrackingFrameAction const nextAction =
        session.SubmitRoiFrame(FrameFromBgra(GrayToBgra(currentGray), 2));
    EXPECT_EQ(nextAction, TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.state == TrackingState::Tracking && snap.sequence == 2u &&
               snap.lastReject == VisualReject::Ok;
    })) << session.FormatReport();

    TransformSnapshot const after = session.Snapshot();
    EXPECT_FALSE(after.hideOverlay);
    EXPECT_GT(after.confidence, 0.0);
    EXPECT_GT(after.inlierCount, 0);
    std::optional<Vec2> const originAfter = Apply(after.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(originAfter.has_value());
    EXPECT_NEAR(originAfter->x - originBefore->x, 8.0, 2.5);
    EXPECT_NEAR(originAfter->y - originBefore->y, 5.0, 2.5);
}

TEST(TrackingSession, WorkerBlankAfterKeyframeDoesNotJump)
{
    TrackingSession session;
    std::string error;
    ASSERT_TRUE(BeginIdentity(session, error)) << error;

    cv::Mat const prior = MakeTexturedGray(0x11CCu);
    TrackingFrameAction const keyAction =
        session.SubmitRoiFrame(FrameFromBgra(GrayToBgra(prior), 1));
    EXPECT_EQ(keyAction, TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.hasKeyframe && snap.sequence == 1u;
    })) << session.FormatReport();

    TransformSnapshot const key = session.Snapshot();
    std::optional<Vec2> const originBefore = Apply(key.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(originBefore.has_value());

    cv::Mat blank(kWorkerHeight, kWorkerWidth, CV_8UC1, cv::Scalar(128));
    TrackingFrameAction const blankAction =
        session.SubmitRoiFrame(FrameFromBgra(GrayToBgra(blank), 2));
    EXPECT_EQ(blankAction, TrackingFrameAction::Enqueue);
    ASSERT_TRUE(WaitUntil(session, [](TransformSnapshot const& snap) {
        return snap.state == TrackingState::Degraded &&
               snap.lastReject == VisualReject::BlankOrLowTexture;
    })) << session.FormatReport();

    TransformSnapshot const after = session.Snapshot();
    EXPECT_EQ(after.sequence, 1u);
    EXPECT_FALSE(after.hideOverlay);
    std::optional<Vec2> const originAfter = Apply(after.mDs, Vec2{0.0, 0.0});
    ASSERT_TRUE(originAfter.has_value());
    EXPECT_NEAR(originBefore->x, originAfter->x, 1.0e-9);
    EXPECT_NEAR(originBefore->y, originAfter->y, 1.0e-9);
}

TEST(TrackingSimilarity, IdentityRoundTripMatchesCalibrationMds)
{
    Transform2D const mDs =
        MakeDocumentToScreen(Vec2{100.0, 200.0}, 0.0, 2.0, false, false, Vec2{0.0, 0.0});
    Transform2D const relative = SimilarityFromEstimate(OkEstimate(), 1);
    EXPECT_NEAR(relative.matrix.m[0], 1.0, 1.0e-12);
    EXPECT_NEAR(relative.matrix.m[6], 0.0, 1.0e-12);
    std::optional<Vec2> const pS = Apply(mDs, Vec2{10.0, 5.0});
    ASSERT_TRUE(pS.has_value());
    EXPECT_NEAR(pS->x, 120.0, 1.0e-9);
    EXPECT_NEAR(pS->y, 210.0, 1.0e-9);
}
