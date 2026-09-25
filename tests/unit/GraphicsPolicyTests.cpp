#include <gtest/gtest.h>

#include "graphics/DeviceResources.h"

namespace {

using tracing::graphics::DeviceLifecycleEvent;
using tracing::graphics::DeviceLifecyclePolicy;
using tracing::graphics::DeviceLifecycleState;
using tracing::graphics::FormatDeviceLifecycleState;
using tracing::graphics::FormatFeatureLevel;

} // namespace

TEST(GraphicsLifecycle, DebugLayerMissingDoesNotFailDevice)
{
    DeviceLifecyclePolicy policy;
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::DebugLayerMissing), DeviceLifecycleState::Uninitialized);
    EXPECT_TRUE(policy.DebugLayerMissing());
    EXPECT_NE(policy.State(), DeviceLifecycleState::Failed);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::CreateSucceeded), DeviceLifecycleState::Ready);
}

TEST(GraphicsLifecycle, HardwareCreateFailureIsFailed)
{
    DeviceLifecyclePolicy policy;
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::CreateFailed), DeviceLifecycleState::Failed);
    EXPECT_FALSE(policy.DebugLayerMissing());
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Retry), DeviceLifecycleState::Failed);
}

TEST(GraphicsLifecycle, DebugLayerMissingStillAllowsLaterCreateFailure)
{
    DeviceLifecyclePolicy policy;
    policy.Apply(DeviceLifecycleEvent::DebugLayerMissing);
    EXPECT_TRUE(policy.DebugLayerMissing());
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::CreateFailed), DeviceLifecycleState::Failed);
}

TEST(GraphicsLifecycle, ReadyRemovedRetryThenReady)
{
    DeviceLifecyclePolicy policy(2);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::CreateSucceeded), DeviceLifecycleState::Ready);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::DeviceRemoved), DeviceLifecycleState::Removed);
    EXPECT_TRUE(policy.CanRetry());
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Retry), DeviceLifecycleState::Retrying);
    EXPECT_EQ(policy.RetryCount(), 1u);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::CreateSucceeded), DeviceLifecycleState::Ready);
}

TEST(GraphicsLifecycle, RetryExhaustionFails)
{
    DeviceLifecyclePolicy policy(0);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::CreateSucceeded), DeviceLifecycleState::Ready);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::DeviceRemoved), DeviceLifecycleState::Removed);
    EXPECT_FALSE(policy.CanRetry());
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Retry), DeviceLifecycleState::Failed);
}

TEST(GraphicsLifecycle, RetryBudgetConsumedThenFails)
{
    DeviceLifecyclePolicy policy(1);
    policy.Apply(DeviceLifecycleEvent::CreateSucceeded);
    policy.Apply(DeviceLifecycleEvent::DeviceRemoved);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Retry), DeviceLifecycleState::Retrying);
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::DeviceRemoved), DeviceLifecycleState::Removed);
    EXPECT_FALSE(policy.CanRetry());
    EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Retry), DeviceLifecycleState::Failed);
}

TEST(GraphicsLifecycle, ShutdownFromEachStateReleases)
{
    {
        DeviceLifecyclePolicy policy;
        EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Shutdown), DeviceLifecycleState::Released);
    }
    {
        DeviceLifecyclePolicy policy;
        policy.Apply(DeviceLifecycleEvent::CreateSucceeded);
        EXPECT_EQ(policy.State(), DeviceLifecycleState::Ready);
        EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Shutdown), DeviceLifecycleState::Released);
    }
    {
        DeviceLifecyclePolicy policy;
        policy.Apply(DeviceLifecycleEvent::CreateSucceeded);
        policy.Apply(DeviceLifecycleEvent::DeviceRemoved);
        EXPECT_EQ(policy.State(), DeviceLifecycleState::Removed);
        EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Shutdown), DeviceLifecycleState::Released);
    }
    {
        DeviceLifecyclePolicy policy;
        policy.Apply(DeviceLifecycleEvent::CreateSucceeded);
        policy.Apply(DeviceLifecycleEvent::DeviceRemoved);
        policy.Apply(DeviceLifecycleEvent::Retry);
        EXPECT_EQ(policy.State(), DeviceLifecycleState::Retrying);
        EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Shutdown), DeviceLifecycleState::Released);
    }
    {
        DeviceLifecyclePolicy policy;
        policy.Apply(DeviceLifecycleEvent::CreateFailed);
        EXPECT_EQ(policy.State(), DeviceLifecycleState::Failed);
        EXPECT_EQ(policy.Apply(DeviceLifecycleEvent::Shutdown), DeviceLifecycleState::Released);
    }
}

TEST(GraphicsFormat, FeatureLevelAndStateLabels)
{
    EXPECT_EQ(FormatFeatureLevel(D3D_FEATURE_LEVEL_11_1), L"11_1");
    EXPECT_EQ(FormatFeatureLevel(D3D_FEATURE_LEVEL_11_0), L"11_0");
    EXPECT_EQ(FormatFeatureLevel(0), L"unknown");
    EXPECT_EQ(FormatDeviceLifecycleState(DeviceLifecycleState::Uninitialized), L"Uninitialized");
    EXPECT_EQ(FormatDeviceLifecycleState(DeviceLifecycleState::Ready), L"Ready");
    EXPECT_EQ(FormatDeviceLifecycleState(DeviceLifecycleState::Removed), L"Removed");
    EXPECT_EQ(FormatDeviceLifecycleState(DeviceLifecycleState::Retrying), L"Retrying");
    EXPECT_EQ(FormatDeviceLifecycleState(DeviceLifecycleState::Failed), L"Failed");
    EXPECT_EQ(FormatDeviceLifecycleState(DeviceLifecycleState::Released), L"Released");
}
