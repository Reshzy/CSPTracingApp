#include <gtest/gtest.h>

#include "platform/AccessibilityObserver.h"

#include <string>
#include <vector>

namespace {

using tracing::platform::AccessibilityCapability;
using tracing::platform::AccessibilityComponent;
using tracing::platform::AccessibilityInspectInput;
using tracing::platform::AccessibilityInspectLimits;
using tracing::platform::AccessibilityNode;
using tracing::platform::AccessibilityObserver;
using tracing::platform::AccessibilityReject;
using tracing::platform::AccessibilityTreeSource;
using tracing::platform::FormatAccessibilityCapability;
using tracing::platform::FormatAccessibilityReport;

constexpr std::uintptr_t kHwnd = 0x100;
constexpr std::uint32_t kPid = 42;
constexpr std::uint64_t kGeneration = 7;

class FakeTreeSource final : public AccessibilityTreeSource
{
public:
    std::vector<AccessibilityNode> nodes;
    std::string openError;
    int nextCalls = 0;

    bool Open(std::string& error) override
    {
        opened_ = false;
        index_ = 0;
        nextCalls = 0;
        if (!openError.empty())
        {
            error = openError;
            return false;
        }
        opened_ = true;
        return true;
    }

    bool Next(AccessibilityNode& node) override
    {
        ++nextCalls;
        if (!opened_ || index_ >= nodes.size())
        {
            return false;
        }
        node = nodes[index_++];
        return true;
    }

    void Close() noexcept override
    {
        opened_ = false;
    }

private:
    bool opened_ = false;
    std::size_t index_ = 0;
};

AccessibilityInspectInput MatchingInput()
{
    AccessibilityInspectInput input{};
    input.attachedGeneration = kGeneration;
    input.sampleGeneration = kGeneration;
    input.sessionAttached = true;
    input.targetAlive = true;
    input.locale = "en-US";
    return input;
}

AccessibilityNode ZoomRange(double value)
{
    AccessibilityNode node{};
    node.name = "Zoom";
    node.automationId = "zoomSlider";
    node.controlType = "slider";
    node.hasRangeValue = true;
    node.rangeFinite = true;
    node.rangeValue = value;
    node.available = true;
    return node;
}

} // namespace

TEST(ObserverCapability, UnprobedDefaultIsDisabled)
{
    AccessibilityObserver observer;
    EXPECT_FALSE(observer.Enabled());
    EXPECT_FALSE(observer.SessionAttached());
    EXPECT_FALSE(observer.Probing());
    EXPECT_EQ(observer.Last().capability, AccessibilityCapability::Unprobed);
    EXPECT_EQ(observer.Last().reject, AccessibilityReject::Disabled);
    EXPECT_TRUE(observer.Last().observed.empty());
    EXPECT_STREQ(FormatAccessibilityCapability(observer.Last().capability), "unprobed");
}

TEST(ObserverCapability, EmptyTreeIsUnsupportedAndStaysDisabled)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    auto const snapshot = observer.Inspect(source, MatchingInput());
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::Unsupported);
    EXPECT_EQ(snapshot.reject, AccessibilityReject::Unsupported);
    EXPECT_FALSE(snapshot.enabled);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_FALSE(snapshot.foundZoom);
    EXPECT_EQ(snapshot.nodesVisited, 0);
    EXPECT_TRUE(snapshot.observed.empty());
}

TEST(ObserverCapability, ProviderOpenFailureIsErrorAndStaysDisabled)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    source.openError = "CoCreateInstance failed";
    auto const snapshot = observer.Inspect(source, MatchingInput());
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::ProviderError);
    EXPECT_EQ(snapshot.reject, AccessibilityReject::ProviderError);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_EQ(snapshot.providerError, "CoCreateInstance failed");
    EXPECT_TRUE(snapshot.observed.empty());
}

TEST(ObserverCapability, ZoomRangeValueIsSupportedOnlyForMatchingGeneration)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    source.nodes.push_back(ZoomRange(50.0));
    auto const snapshot = observer.Inspect(source, MatchingInput());
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::Supported);
    EXPECT_EQ(snapshot.reject, AccessibilityReject::Ok);
    EXPECT_TRUE(snapshot.foundZoom);
    EXPECT_FALSE(snapshot.foundRotation);
    EXPECT_FALSE(snapshot.foundNavigator);
    EXPECT_TRUE(observer.Enabled());
    ASSERT_EQ(snapshot.observed.size(), 1u);
    EXPECT_EQ(snapshot.observed[0].component, AccessibilityComponent::Zoom);
    EXPECT_TRUE(snapshot.observed[0].hasNumeric);
    EXPECT_DOUBLE_EQ(snapshot.observed[0].numeric, 50.0);
    EXPECT_NE(FormatAccessibilityReport(snapshot).find("foundZoom=yes"), std::string::npos);
}

TEST(ObserverCapability, GenerationChangeDropsCacheAndDisables)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    source.nodes.push_back(ZoomRange(80.0));
    ASSERT_EQ(
        observer.Inspect(source, MatchingInput()).capability,
        AccessibilityCapability::Supported);
    EXPECT_TRUE(observer.Enabled());

    AccessibilityInspectInput next = MatchingInput();
    next.sampleGeneration = kGeneration + 1;
    auto const stale = observer.Inspect(source, next);
    EXPECT_EQ(stale.capability, AccessibilityCapability::Stale);
    EXPECT_EQ(stale.reject, AccessibilityReject::GenerationChanged);
    EXPECT_FALSE(stale.enabled);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_TRUE(stale.observed.empty());
    EXPECT_FALSE(stale.foundZoom);
}

TEST(ObserverCapability, TargetClosedDropsCacheAndDisables)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    source.nodes.push_back(ZoomRange(25.0));
    ASSERT_TRUE(observer.Inspect(source, MatchingInput()).enabled);

    AccessibilityInspectInput closed = MatchingInput();
    closed.targetAlive = false;
    auto const snapshot = observer.Inspect(source, closed);
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::TargetClosed);
    EXPECT_EQ(snapshot.reject, AccessibilityReject::TargetClosed);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_TRUE(snapshot.observed.empty());
}

TEST(ObserverCapability, UnavailableNodeMidWalkIsStaleNotFabricatedValue)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    AccessibilityNode dead{};
    dead.name = "Zoom";
    dead.hasRangeValue = true;
    dead.rangeFinite = true;
    dead.rangeValue = 99.0;
    dead.available = false;
    source.nodes.push_back(dead);
    auto const snapshot = observer.Inspect(source, MatchingInput());
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::Stale);
    EXPECT_EQ(snapshot.reject, AccessibilityReject::Stale);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_FALSE(snapshot.foundZoom);
    EXPECT_TRUE(snapshot.observed.empty());
    EXPECT_EQ(snapshot.nodesVisited, 1);
}

TEST(ObserverCapability, KeywordNameWithoutValueRemainsUnsupported)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    AccessibilityNode node{};
    node.name = "Zoom";
    node.automationId = "zoomLabel";
    node.controlType = "text";
    node.available = true;
    source.nodes.push_back(node);
    auto const snapshot = observer.Inspect(source, MatchingInput());
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::Unsupported);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_FALSE(snapshot.foundZoom);
    EXPECT_TRUE(snapshot.observed.empty());
    EXPECT_EQ(snapshot.nodesVisited, 1);
}

TEST(ObserverCapability, MaxNodesClassifiesBoundedPrefixWithoutHanging)
{
    AccessibilityInspectLimits limits{};
    limits.maxNodes = 2;
    limits.timeout = std::chrono::milliseconds{400};
    limits.maxObserved = 8;
    AccessibilityObserver observer(limits);
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    source.nodes.push_back(ZoomRange(10.0));
    AccessibilityNode other{};
    other.name = "Palette";
    other.available = true;
    source.nodes.push_back(other);
    source.nodes.push_back(ZoomRange(90.0));
    auto const snapshot = observer.Inspect(source, MatchingInput());
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::Supported);
    EXPECT_TRUE(observer.Enabled());
    EXPECT_EQ(snapshot.nodesVisited, 2);
    EXPECT_TRUE(snapshot.truncated);
    EXPECT_EQ(source.nextCalls, 3);
    ASSERT_EQ(snapshot.observed.size(), 1u);
    EXPECT_DOUBLE_EQ(snapshot.observed[0].numeric, 10.0);
}

TEST(ObserverCapability, NoSessionInspectKeepsDisabled)
{
    AccessibilityObserver observer;
    FakeTreeSource source;
    source.nodes.push_back(ZoomRange(40.0));
    AccessibilityInspectInput input = MatchingInput();
    input.sessionAttached = false;
    auto const snapshot = observer.Inspect(source, input);
    EXPECT_EQ(snapshot.capability, AccessibilityCapability::Unsupported);
    EXPECT_EQ(snapshot.reject, AccessibilityReject::NoSession);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_TRUE(snapshot.observed.empty());
}

TEST(ObserverCapability, MarkTargetClosedClearsEnabledAdapter)
{
    AccessibilityObserver observer;
    observer.AttachSession(kHwnd, kPid, kGeneration);
    FakeTreeSource source;
    source.nodes.push_back(ZoomRange(12.0));
    ASSERT_TRUE(observer.Inspect(source, MatchingInput()).enabled);
    observer.MarkTargetClosed();
    EXPECT_EQ(observer.Last().capability, AccessibilityCapability::TargetClosed);
    EXPECT_FALSE(observer.Enabled());
    EXPECT_TRUE(observer.Last().observed.empty());
}
