#include <gtest/gtest.h>

#include "capture/CaptureSession.h"

namespace {

using tracing::capture::CaptureFrameArrival;
using tracing::capture::CaptureHandoffAction;
using tracing::capture::CaptureHandoffResult;
using tracing::capture::CaptureSessionEvent;
using tracing::capture::CaptureSessionPolicy;
using tracing::capture::CaptureSessionState;
using tracing::capture::FormatCaptureHandoffAction;
using tracing::capture::FormatCaptureSessionState;

CaptureFrameArrival ValidArrival(std::uint64_t generation, int width = 256, int height = 128)
{
    CaptureFrameArrival arrival{};
    arrival.targetGeneration = generation;
    arrival.contentWidth = width;
    arrival.contentHeight = height;
    return arrival;
}

} // namespace

TEST(CaptureLifecycle, SupportMissingIsUnsupportedNotFailed)
{
    CaptureSessionPolicy policy;
    EXPECT_EQ(policy.Apply(CaptureSessionEvent::SupportMissing), CaptureSessionState::Unsupported);
    EXPECT_NE(policy.State(), CaptureSessionState::Failed);
    EXPECT_EQ(FormatCaptureSessionState(policy.State()), L"Unsupported");
}

TEST(CaptureLifecycle, StartThenStopRejectsFurtherFrames)
{
    CaptureSessionPolicy policy;
    EXPECT_EQ(policy.Apply(CaptureSessionEvent::SupportPresent), CaptureSessionState::Starting);
    EXPECT_EQ(policy.Apply(CaptureSessionEvent::StartSucceeded), CaptureSessionState::Running);
    policy.SetSessionGeneration(7);
    CaptureHandoffResult const first = policy.Arrive(ValidArrival(7));
    EXPECT_EQ(first.action, CaptureHandoffAction::Enqueue);
    EXPECT_EQ(first.sequence, 1u);
    EXPECT_EQ(policy.Accepted(), 1u);
    EXPECT_EQ(policy.Apply(CaptureSessionEvent::Stop), CaptureSessionState::Stopped);
    EXPECT_FALSE(policy.GenerationValid());
    CaptureHandoffResult const later = policy.Arrive(ValidArrival(7));
    EXPECT_EQ(later.action, CaptureHandoffAction::RejectNotRunning);
    EXPECT_EQ(later.sequence, 0u);
    EXPECT_EQ(policy.Accepted(), 1u);
    EXPECT_EQ(FormatCaptureHandoffAction(later.action), L"reject-not-running");
}

TEST(CaptureHandoff, WrongGenerationRejectedLeavesAcceptedUnchanged)
{
    CaptureSessionPolicy policy;
    policy.Apply(CaptureSessionEvent::StartSucceeded);
    policy.SetSessionGeneration(3);
    CaptureHandoffResult const ok = policy.Arrive(ValidArrival(3));
    EXPECT_EQ(ok.action, CaptureHandoffAction::Enqueue);
    EXPECT_EQ(policy.Accepted(), 1u);
    CaptureHandoffResult const bad = policy.Arrive(ValidArrival(4));
    EXPECT_EQ(bad.action, CaptureHandoffAction::RejectWrongGeneration);
    EXPECT_EQ(bad.sequence, 0u);
    EXPECT_EQ(policy.Accepted(), 1u);
    EXPECT_EQ(policy.RejectedGeneration(), 1u);
    EXPECT_EQ(policy.LastSequence(), 1u);
}

TEST(CaptureLifecycle, ItemClosedInvalidatesGeneration)
{
    CaptureSessionPolicy policy;
    policy.Apply(CaptureSessionEvent::StartSucceeded);
    policy.SetSessionGeneration(5);
    EXPECT_TRUE(policy.GenerationValid());
    EXPECT_EQ(policy.Apply(CaptureSessionEvent::ItemClosed), CaptureSessionState::ItemClosed);
    EXPECT_FALSE(policy.GenerationValid());
    CaptureHandoffResult const rejected = policy.Arrive(ValidArrival(5));
    EXPECT_EQ(rejected.action, CaptureHandoffAction::RejectNotRunning);
    EXPECT_EQ(policy.Accepted(), 0u);
}

TEST(CaptureHandoff, PendingBoundDropsOldestOnThirdArrival)
{
    CaptureSessionPolicy policy;
    policy.Apply(CaptureSessionEvent::StartSucceeded);
    policy.SetSessionGeneration(1);
    CaptureHandoffResult const first = policy.Arrive(ValidArrival(1));
    CaptureHandoffResult const second = policy.Arrive(ValidArrival(1));
    EXPECT_EQ(first.action, CaptureHandoffAction::Enqueue);
    EXPECT_EQ(second.action, CaptureHandoffAction::Enqueue);
    EXPECT_EQ(policy.Pending(), 2u);
    CaptureHandoffResult const third = policy.Arrive(ValidArrival(1));
    EXPECT_EQ(third.action, CaptureHandoffAction::DropOldestThenEnqueue);
    EXPECT_EQ(third.sequence, 3u);
    EXPECT_EQ(policy.Pending(), 2u);
    EXPECT_EQ(policy.DroppedBound(), 1u);
    EXPECT_EQ(policy.Accepted(), 3u);
}

TEST(CaptureHandoff, ZeroContentSizeIsStaleNotAccepted)
{
    CaptureSessionPolicy policy;
    policy.Apply(CaptureSessionEvent::StartSucceeded);
    policy.SetSessionGeneration(2);
    CaptureFrameArrival zero{};
    zero.targetGeneration = 2;
    zero.contentWidth = 0;
    zero.contentHeight = 10;
    CaptureHandoffResult const stale = policy.Arrive(zero);
    EXPECT_EQ(stale.action, CaptureHandoffAction::StaleZeroSize);
    EXPECT_EQ(stale.sequence, 0u);
    EXPECT_EQ(policy.Accepted(), 0u);
    EXPECT_EQ(policy.Stale(), 1u);
    EXPECT_EQ(policy.Pending(), 0u);
    EXPECT_EQ(policy.LastSequence(), 0u);
}

TEST(CaptureHandoff, SequenceIncrementsOnlyOnAccept)
{
    CaptureSessionPolicy policy;
    policy.Apply(CaptureSessionEvent::StartSucceeded);
    policy.SetSessionGeneration(9);
    EXPECT_EQ(policy.Arrive(ValidArrival(9)).sequence, 1u);
    CaptureFrameArrival zero{};
    zero.targetGeneration = 9;
    zero.contentWidth = 64;
    zero.contentHeight = 0;
    EXPECT_EQ(policy.Arrive(zero).sequence, 0u);
    EXPECT_EQ(policy.Arrive(ValidArrival(8)).sequence, 0u);
    EXPECT_EQ(policy.Arrive(ValidArrival(9)).sequence, 2u);
    EXPECT_EQ(policy.Accepted(), 2u);
    EXPECT_EQ(policy.Stale(), 1u);
    EXPECT_EQ(policy.RejectedGeneration(), 1u);
    EXPECT_EQ(policy.LastSequence(), 2u);
}

TEST(CaptureFormat, StateAndActionLabels)
{
    EXPECT_EQ(FormatCaptureSessionState(CaptureSessionState::Idle), L"Idle");
    EXPECT_EQ(FormatCaptureSessionState(CaptureSessionState::Starting), L"Starting");
    EXPECT_EQ(FormatCaptureSessionState(CaptureSessionState::Running), L"Running");
    EXPECT_EQ(FormatCaptureSessionState(CaptureSessionState::ItemClosed), L"ItemClosed");
    EXPECT_EQ(FormatCaptureSessionState(CaptureSessionState::Failed), L"Failed");
    EXPECT_EQ(FormatCaptureSessionState(CaptureSessionState::Stopped), L"Stopped");
    EXPECT_EQ(FormatCaptureHandoffAction(CaptureHandoffAction::Enqueue), L"enqueue");
    EXPECT_EQ(
        FormatCaptureHandoffAction(CaptureHandoffAction::DropOldestThenEnqueue),
        L"drop-oldest-then-enqueue");
}
