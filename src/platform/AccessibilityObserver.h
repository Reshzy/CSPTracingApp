#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tracing::platform {

// Optional UI Automation capability probe. Read-only: no Invoke/SetValue, no
// tracking fusion, no hardcoded CSP support claim. Unsupported is valid.

inline constexpr int kMaxAccessibilityNodes = 256;
inline constexpr int kMaxAccessibilityObserved = 8;
inline constexpr std::chrono::milliseconds kMaxAccessibilityProbe{400};

enum class AccessibilityCapability
{
    Unprobed,
    Probing,
    Unsupported,
    Supported,
    Stale,
    ProviderError,
    TargetClosed,
};

enum class AccessibilityReject
{
    Ok,
    Disabled,
    NoSession,
    Unsupported,
    ProviderError,
    TargetClosed,
    Stale,
    GenerationChanged,
};

enum class AccessibilityComponent
{
    None,
    Zoom,
    Rotation,
    Navigator,
};

struct AccessibilityNode
{
    std::string name;
    std::string automationId;
    std::string controlType;
    bool hasRangeValue = false;
    bool hasValue = false;
    bool rangeFinite = false;
    double rangeValue = 0.0;
    std::string valueText;
    bool available = true;
};

struct AccessibilityObservedProperty
{
    AccessibilityComponent component = AccessibilityComponent::None;
    std::string name;
    std::string automationId;
    std::string controlType;
    bool hasNumeric = false;
    double numeric = 0.0;
};

struct AccessibilityInspectLimits
{
    int maxNodes = kMaxAccessibilityNodes;
    std::chrono::milliseconds timeout = kMaxAccessibilityProbe;
    int maxObserved = kMaxAccessibilityObserved;
};

struct AccessibilityInspectInput
{
    std::uint64_t attachedGeneration = 0;
    std::uint64_t sampleGeneration = 0;
    bool sessionAttached = false;
    bool targetAlive = true;
    std::string locale;
};

struct AccessibilitySnapshot
{
    AccessibilityCapability capability = AccessibilityCapability::Unprobed;
    AccessibilityReject reject = AccessibilityReject::Disabled;
    std::uint64_t targetGeneration = 0;
    int nodesVisited = 0;
    bool foundZoom = false;
    bool foundRotation = false;
    bool foundNavigator = false;
    bool enabled = false;
    bool truncated = false;
    std::string locale;
    std::string providerError;
    std::vector<AccessibilityObservedProperty> observed;
};

class AccessibilityTreeSource
{
public:
    virtual ~AccessibilityTreeSource() = default;
    virtual bool Open(std::string& error) = 0;
    virtual bool Next(AccessibilityNode& node) = 0;
    virtual void Close() noexcept = 0;
};

char const* FormatAccessibilityCapability(AccessibilityCapability capability) noexcept;
char const* FormatAccessibilityReject(AccessibilityReject reject) noexcept;
char const* FormatAccessibilityComponent(AccessibilityComponent component) noexcept;
std::string FormatAccessibilityReport(AccessibilitySnapshot const& snapshot);

class AccessibilityObserver
{
public:
    explicit AccessibilityObserver(AccessibilityInspectLimits limits = {});
    ~AccessibilityObserver();

    AccessibilityObserver(AccessibilityObserver const&) = delete;
    AccessibilityObserver& operator=(AccessibilityObserver const&) = delete;

    void AttachSession(std::uintptr_t hwnd, std::uint32_t pid, std::uint64_t generation) noexcept;
    void Detach() noexcept;
    void MarkTargetClosed() noexcept;

    AccessibilitySnapshot Inspect(
        AccessibilityTreeSource& source,
        AccessibilityInspectInput const& input);

    bool BeginProbe(std::uintptr_t notifyHwnd, unsigned notifyMessage);
    bool ApplyCompletedProbe(std::uint64_t postedGeneration);

    AccessibilitySnapshot const& Last() const noexcept;
    bool Enabled() const noexcept;
    bool SessionAttached() const noexcept;
    bool Probing() const noexcept;

private:
    AccessibilitySnapshot BuildSnapshot(
        AccessibilityTreeSource& source,
        AccessibilityInspectInput const& input,
        std::atomic<bool> const* cancel) const;
    AccessibilitySnapshot MakeDisabled(
        AccessibilityCapability capability,
        AccessibilityReject reject,
        std::uint64_t generation) const;
    void CancelAndJoin() noexcept;
    void WorkerMain(
        std::uintptr_t hwnd,
        std::uint32_t pid,
        std::uint64_t generation,
        std::uintptr_t notifyHwnd,
        unsigned notifyMessage);

    AccessibilityInspectLimits limits_{};
    bool sessionAttached_ = false;
    std::uintptr_t sessionHwnd_ = 0;
    std::uint32_t sessionPid_ = 0;
    std::uint64_t sessionGeneration_ = 0;
    AccessibilitySnapshot last_{};
    std::atomic<bool> cancel_{false};
    std::atomic<bool> probing_{false};
    std::thread worker_{};
    std::mutex pendingMutex_{};
    AccessibilitySnapshot pending_{};
    bool pendingReady_ = false;
};

} // namespace tracing::platform
