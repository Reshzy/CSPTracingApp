#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace tracing::diagnostics {

// Bounded diagnostics. Independent of Windows, OpenCV, and rendering.
// OBS verification is user-recorded and is never derived from affinity API success.

inline constexpr std::size_t kMaxMetricSamples = 256;
inline constexpr std::size_t kMaxLogEvents = 128;
inline constexpr std::size_t kMaxLogEventChars = 240;
inline constexpr std::uint64_t kMaxReplayBytes = 32ull * 1024ull * 1024ull;
inline constexpr std::size_t kMaxReplayFiles = 256;

enum class ObsVerificationStatus
{
    NotRun,
    Unverified,
    UserPass,
    UserFail,
};

enum class ReplayReject
{
    Ok,
    Disabled,
    SizeLimit,
    WriteFailed,
    EmptyDirectory,
};

struct MetricsSample
{
    std::int64_t captureAgeMs = 0;
    std::uint64_t droppedBound = 0;
    std::uint64_t stagingWaitUs = 0;
    std::int64_t trackingLagMs = 0;
    double confidence = 0.0;
    int inliers = 0;
    double residualPx = 0.0;
    std::string parity;
    std::string trackingState;
    unsigned dpi = 0;
    int roiW = 0;
    int roiH = 0;
    std::uint32_t deviceHr = 0;
    std::uint32_t deviceRemovedHr = 0;
    bool affinityOk = false;
};

struct PercentileSummary
{
    std::size_t count = 0;
    double p50 = 0.0;
    double p95 = 0.0;
    double max = 0.0;
};

struct MetricsSummary
{
    std::size_t sampleCount = 0;
    PercentileSummary captureAgeMs{};
    PercentileSummary stagingWaitUs{};
    PercentileSummary trackingLagMs{};
    PercentileSummary confidence{};
    PercentileSummary residualPx{};
    std::uint64_t lastDroppedBound = 0;
    unsigned lastDpi = 0;
    int lastRoiW = 0;
    int lastRoiH = 0;
    std::string lastParity;
    std::string lastTrackingState;
    bool lastAffinityOk = false;
    std::uint32_t lastDeviceHr = 0;
    std::uint32_t lastDeviceRemovedHr = 0;
    ObsVerificationStatus obsVerify = ObsVerificationStatus::NotRun;
    bool previewEnabled = false;
    bool replayEnabled = false;
    std::uint64_t replayBytes = 0;
    std::uint64_t replayLimit = 0;
    bool replayStoppedSizeLimit = false;
};

// Nearest-rank percentile. p in [0, 1]. Empty input returns 0 (not fabricated).
// Rank = ceil(p * n), 1-based; p <= 0 returns the minimum.
double Percentile(std::vector<double> values, double p);

char const* FormatObsVerificationStatus(ObsVerificationStatus status) noexcept;
char const* FormatReplayReject(ReplayReject reject) noexcept;

std::string SanitizeLogText(std::string const& text);
bool ParseU64Field(std::wstring const& report, std::wstring const& key, std::uint64_t& out);
std::string FormatMetricsSampleLine(MetricsSample const& sample);

class MetricsCollector
{
public:
    void RecordSample(MetricsSample const& sample);
    void RecordAffinity(bool affinityOk) noexcept;
    void SetObsVerification(ObsVerificationStatus status) noexcept;
    ObsVerificationStatus ObsVerification() const noexcept;
    void SetPreviewEnabled(bool enabled) noexcept;
    void NoteReplayStatus(
        bool enabled,
        std::uint64_t bytes,
        std::uint64_t limit,
        bool stoppedSizeLimit) noexcept;
    void AppendEvent(std::string const& text);

    std::size_t SampleCount() const noexcept;
    std::size_t EventCount() const noexcept;
    std::string const& OldestEvent() const;
    std::string const& NewestEvent() const;
    MetricsSummary Summarize() const;
    std::string FormatSummary() const;

private:
    std::deque<MetricsSample> samples_;
    std::deque<std::string> events_;
    ObsVerificationStatus obsVerify_ = ObsVerificationStatus::NotRun;
    bool previewEnabled_ = false;
    bool replayEnabled_ = false;
    std::uint64_t replayBytes_ = 0;
    std::uint64_t replayLimit_ = 0;
    bool replayStoppedSizeLimit_ = false;
    static std::string const kEmptyEvent;
};

class ReplayRecorder
{
public:
    explicit ReplayRecorder(
        std::uint64_t maxBytes = kMaxReplayBytes,
        std::size_t maxFiles = kMaxReplayFiles);

    void SetDirectory(std::string directory);
    void SetEnabled(bool enabled) noexcept;
    bool Enabled() const noexcept;
    std::uint64_t BytesWritten() const noexcept;
    std::uint64_t MaxBytes() const noexcept;
    std::size_t FilesWritten() const noexcept;
    bool StoppedSizeLimit() const noexcept;
    ReplayReject LastReject() const noexcept;

    ReplayReject RecordNumericLine(std::string const& line);
    ReplayReject RecordImageFrame(
        std::uint64_t sequence,
        std::uint8_t const* bgra,
        std::size_t byteCount);

    std::string FormatStatus() const;

private:
    ReplayReject TryReserve(std::uint64_t bytes) noexcept;
    bool WriteAll(std::string const& path, void const* data, std::size_t byteCount);

    std::string directory_;
    bool enabled_ = false;
    std::uint64_t maxBytes_ = kMaxReplayBytes;
    std::size_t maxFiles_ = kMaxReplayFiles;
    std::uint64_t bytesWritten_ = 0;
    std::size_t filesWritten_ = 0;
    bool stoppedSizeLimit_ = false;
    ReplayReject lastReject_ = ReplayReject::Disabled;
};

} // namespace tracing::diagnostics
