#include <gtest/gtest.h>

#include "graphics/OverlaySurface.h"

namespace {

using tracing::graphics::AffinityIsReady;
using tracing::graphics::DecideOverlayVisibility;
using tracing::graphics::FormatObsVerification;
using tracing::graphics::FormatOverlayVisibilityDecision;
using tracing::graphics::FormatOverlayVisibilityReason;
using tracing::graphics::OverlayAffinityStatus;
using tracing::graphics::OverlayObsVerification;
using tracing::graphics::OverlayVisibilityDecision;
using tracing::graphics::OverlayVisibilityInput;
using tracing::graphics::OverlayVisibilityReason;

OverlayVisibilityInput EligibleInput()
{
    OverlayVisibilityInput input{};
    input.targetUsable = true;
    input.targetForeground = true;
    input.affinityOk = true;
    input.contentReady = true;
    return input;
}

} // namespace

TEST(OverlayVisibility, EligibleTargetShows)
{
    auto const result = DecideOverlayVisibility(EligibleInput());
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Show);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::ShownOnTarget);
    EXPECT_EQ(FormatOverlayVisibilityDecision(result.decision), L"Show");
}

TEST(OverlayVisibility, ControlForegroundWithUsableTargetShows)
{
    OverlayVisibilityInput input = EligibleInput();
    input.targetForeground = false;
    input.ownerControlForeground = true;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Show);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::ShownOnTarget);
}

TEST(OverlayVisibility, UnrelatedForegroundHides)
{
    OverlayVisibilityInput input = EligibleInput();
    input.targetForeground = false;
    input.ownerControlForeground = false;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::UnrelatedForeground);
    EXPECT_EQ(FormatOverlayVisibilityReason(result.reason), L"unrelated-foreground");
}

TEST(OverlayVisibility, UnusableTargetHides)
{
    OverlayVisibilityInput input = EligibleInput();
    input.targetUsable = false;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::TargetUnusable);
}

TEST(OverlayVisibility, AffinityFailureHidesEvenIfEligible)
{
    OverlayVisibilityInput input = EligibleInput();
    input.affinityOk = false;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::AffinityFailed);
}

TEST(OverlayVisibility, EmergencyHideLatchesHide)
{
    OverlayVisibilityInput input = EligibleInput();
    input.emergencyHidden = true;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::EmergencyHidden);
}

TEST(OverlayVisibility, ClearingEmergencyAllowsShow)
{
    OverlayVisibilityInput hidden = EligibleInput();
    hidden.emergencyHidden = true;
    EXPECT_EQ(DecideOverlayVisibility(hidden).decision, OverlayVisibilityDecision::Hide);

    OverlayVisibilityInput shown = EligibleInput();
    shown.emergencyHidden = false;
    auto const result = DecideOverlayVisibility(shown);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Show);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::ShownOnTarget);
}

TEST(OverlayVisibility, ContentNotReadyHides)
{
    OverlayVisibilityInput input = EligibleInput();
    input.contentReady = false;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::ContentNotReady);
}

TEST(OverlayVisibility, TestPatternWithoutTargetShowsOnlyWhenFlagSet)
{
    OverlayVisibilityInput withoutFlag{};
    withoutFlag.affinityOk = true;
    withoutFlag.contentReady = true;
    withoutFlag.testPatternWithoutTarget = false;
    EXPECT_EQ(DecideOverlayVisibility(withoutFlag).decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(DecideOverlayVisibility(withoutFlag).reason, OverlayVisibilityReason::TargetUnusable);

    OverlayVisibilityInput withFlag = withoutFlag;
    withFlag.testPatternWithoutTarget = true;
    auto const result = DecideOverlayVisibility(withFlag);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Show);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::ShownTestPattern);
}

TEST(OverlayVisibility, TestPatternDoesNotOverrideUnrelatedForeground)
{
    OverlayVisibilityInput input = EligibleInput();
    input.targetForeground = false;
    input.ownerControlForeground = false;
    input.testPatternWithoutTarget = true;
    auto const result = DecideOverlayVisibility(input);
    EXPECT_EQ(result.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(result.reason, OverlayVisibilityReason::UnrelatedForeground);
}

TEST(OverlayVisibility, ObsNotRunIsNotAnInputAndDoesNotAuthorizeShow)
{
    EXPECT_EQ(FormatObsVerification(OverlayObsVerification::NotRun), L"NOT RUN");

    OverlayVisibilityInput eligible = EligibleInput();
    auto const shown = DecideOverlayVisibility(eligible);
    EXPECT_EQ(shown.decision, OverlayVisibilityDecision::Show);

    OverlayVisibilityInput emergency = eligible;
    emergency.emergencyHidden = true;
    auto const hidden = DecideOverlayVisibility(emergency);
    EXPECT_EQ(hidden.decision, OverlayVisibilityDecision::Hide);
    EXPECT_EQ(hidden.reason, OverlayVisibilityReason::EmergencyHidden);

    OverlayVisibilityInput affinityFail = eligible;
    affinityFail.affinityOk = false;
    EXPECT_EQ(DecideOverlayVisibility(affinityFail).decision, OverlayVisibilityDecision::Hide);
}

TEST(OverlayAffinity, ReadyRequiresSetAndMatchingReadback)
{
    OverlayAffinityStatus status{};
    EXPECT_FALSE(AffinityIsReady(status));

    status.setCalled = true;
    status.setResult = 1;
    status.matchesExcludeFromCapture = false;
    EXPECT_FALSE(AffinityIsReady(status));

    status.matchesExcludeFromCapture = true;
    EXPECT_TRUE(AffinityIsReady(status));

    status.setResult = 0;
    EXPECT_FALSE(AffinityIsReady(status));
}

TEST(OverlayAffinity, TemporaryNoneReadyOnlyOnNoneReadback)
{
    using tracing::graphics::AffinityIsReadyForMode;
    using tracing::graphics::FormatOverlayAffinityMode;
    using tracing::graphics::OverlayAffinityMode;

    OverlayAffinityStatus status{};
    status.setCalled = true;
    status.setResult = 1;
    status.matchesExcludeFromCapture = true;
    status.matchesNone = false;
    EXPECT_TRUE(AffinityIsReady(status));
    EXPECT_FALSE(AffinityIsReadyForMode(
        status, OverlayAffinityMode::TemporaryNonePositiveControl));
    EXPECT_EQ(
        FormatOverlayAffinityMode(OverlayAffinityMode::TemporaryNonePositiveControl),
        L"temporary-none-positive-control");

    status.matchesExcludeFromCapture = false;
    status.matchesNone = true;
    EXPECT_FALSE(AffinityIsReady(status));
    EXPECT_TRUE(AffinityIsReadyForMode(
        status, OverlayAffinityMode::TemporaryNonePositiveControl));
    EXPECT_FALSE(AffinityIsReadyForMode(status, OverlayAffinityMode::ExcludeFromCapture));

    status.matchesNone = false;
    EXPECT_FALSE(AffinityIsReadyForMode(
        status, OverlayAffinityMode::TemporaryNonePositiveControl));
}

TEST(OverlayAffinity, RestoredExcludeRequiresExcludeReadback)
{
    using tracing::graphics::AffinityIsReadyForMode;
    using tracing::graphics::FormatOverlayAffinityMode;
    using tracing::graphics::OverlayAffinityMode;

    OverlayAffinityStatus status{};
    status.setCalled = true;
    status.setResult = 1;
    status.matchesNone = true;
    EXPECT_FALSE(AffinityIsReadyForMode(status, OverlayAffinityMode::ExcludeFromCapture));
    EXPECT_EQ(
        FormatOverlayAffinityMode(OverlayAffinityMode::ExcludeFromCapture),
        L"exclude-from-capture");

    status.matchesNone = false;
    status.matchesExcludeFromCapture = true;
    EXPECT_TRUE(AffinityIsReadyForMode(status, OverlayAffinityMode::ExcludeFromCapture));
    EXPECT_FALSE(AffinityIsReadyForMode(
        status, OverlayAffinityMode::TemporaryNonePositiveControl));
}
