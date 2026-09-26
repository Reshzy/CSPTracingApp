#include "diagnostics/Metrics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tracing::diagnostics {
namespace {

bool IsHexDigit(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool ContainsInsensitive(std::string const& text, char const* needle)
{
    if (needle == nullptr || *needle == '\0')
    {
        return false;
    }
    auto const it = std::search(
        text.begin(),
        text.end(),
        needle,
        needle + std::strlen(needle),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
    return it != text.end();
}

bool IsPathStart(std::string const& text, std::size_t i) noexcept
{
    if (i + 2 < text.size() && std::isalpha(static_cast<unsigned char>(text[i])) &&
        text[i + 1] == ':' && (text[i + 2] == '\\' || text[i + 2] == '/'))
    {
        return true;
    }
    return i + 1 < text.size() && text[i] == '\\' && text[i + 1] == '\\';
}

std::size_t PathEnd(std::string const& text, std::size_t i) noexcept
{
    std::size_t j = i;
    while (j < text.size())
    {
        unsigned char const c = static_cast<unsigned char>(text[j]);
        if (std::isspace(c) || c == '"' || c == '\'')
        {
            break;
        }
        ++j;
    }
    return j;
}

std::string LeafOrPlaceholder(std::string const& path)
{
    auto const slash = path.find_last_of("\\/");
    if (slash == std::string::npos || slash + 1 >= path.size())
    {
        return "<path>";
    }
    return path.substr(slash + 1);
}

std::string ReplaceHexRuns(std::string const& text, char const* token)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size())
    {
        if (IsHexDigit(text[i]))
        {
            std::size_t j = i;
            while (j < text.size() && IsHexDigit(text[j]))
            {
                ++j;
            }
            if (j - i >= 32)
            {
                out += token;
                i = j;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

PercentileSummary MakePercentileSummary(std::vector<double> const& values)
{
    PercentileSummary summary{};
    summary.count = values.size();
    if (values.empty())
    {
        return summary;
    }
    summary.p50 = Percentile(values, 0.50);
    summary.p95 = Percentile(values, 0.95);
    summary.max = Percentile(values, 1.00);
    return summary;
}

void AppendHex32(std::string& text, std::uint32_t value)
{
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "0x%08X", value);
    text += buffer;
}

void AppendDouble(std::string& text, char const* format, double value)
{
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), format, value);
    text += buffer;
}

} // namespace

std::string const MetricsCollector::kEmptyEvent{};

double Percentile(std::vector<double> values, double p)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    if (p <= 0.0)
    {
        return values.front();
    }
    if (p >= 1.0)
    {
        return values.back();
    }
    double const rank = std::ceil(p * static_cast<double>(values.size()));
    std::size_t index = static_cast<std::size_t>(rank);
    if (index < 1)
    {
        index = 1;
    }
    if (index > values.size())
    {
        index = values.size();
    }
    return values[index - 1];
}

char const* FormatObsVerificationStatus(ObsVerificationStatus status) noexcept
{
    switch (status)
    {
    case ObsVerificationStatus::NotRun:
        return "NOT RUN";
    case ObsVerificationStatus::Unverified:
        return "unverified";
    case ObsVerificationStatus::UserPass:
        return "user-PASS";
    case ObsVerificationStatus::UserFail:
        return "user-FAIL";
    }
    return "NOT RUN";
}

char const* FormatReplayReject(ReplayReject reject) noexcept
{
    switch (reject)
    {
    case ReplayReject::Ok:
        return "ok";
    case ReplayReject::Disabled:
        return "disabled";
    case ReplayReject::SizeLimit:
        return "stopped-size-limit";
    case ReplayReject::WriteFailed:
        return "write-failed";
    case ReplayReject::EmptyDirectory:
        return "empty-directory";
    }
    return "unknown";
}

std::string SanitizeLogText(std::string const& text)
{
    std::string withoutPaths;
    withoutPaths.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size())
    {
        if (IsPathStart(text, i))
        {
            std::size_t const end = PathEnd(text, i);
            withoutPaths += LeafOrPlaceholder(text.substr(i, end - i));
            i = end;
            continue;
        }
        withoutPaths.push_back(text[i]);
        ++i;
    }

    bool const rawInput = ContainsInsensitive(withoutPaths, "raw input") ||
                          ContainsInsensitive(withoutPaths, "rawInput") ||
                          ContainsInsensitive(withoutPaths, "hid report");
    if (rawInput)
    {
        return ReplaceHexRuns(withoutPaths, "<raw-input>");
    }
    if (ContainsInsensitive(withoutPaths, "bgra") || ContainsInsensitive(withoutPaths, "pixels="))
    {
        return ReplaceHexRuns(withoutPaths, "<pixels>");
    }
    return ReplaceHexRuns(withoutPaths, "<pixels>");
}

bool ParseU64Field(std::wstring const& report, std::wstring const& key, std::uint64_t& out)
{
    out = 0;
    if (key.empty())
    {
        return false;
    }
    std::size_t const pos = report.find(key);
    if (pos == std::wstring::npos)
    {
        return false;
    }
    std::size_t i = pos + key.size();
    while (i < report.size() && (report[i] == L' ' || report[i] == L'\t'))
    {
        ++i;
    }
    if (i >= report.size() || report[i] < L'0' || report[i] > L'9')
    {
        return false;
    }
    std::uint64_t value = 0;
    bool any = false;
    while (i < report.size() && report[i] >= L'0' && report[i] <= L'9')
    {
        std::uint64_t const digit = static_cast<std::uint64_t>(report[i] - L'0');
        if (value > (UINT64_MAX - digit) / 10ull)
        {
            return false;
        }
        value = value * 10ull + digit;
        any = true;
        ++i;
    }
    if (!any)
    {
        return false;
    }
    out = value;
    return true;
}

std::string FormatMetricsSampleLine(MetricsSample const& sample)
{
    std::string text = "captureAgeMs=";
    text += std::to_string(sample.captureAgeMs);
    text += " droppedBound=";
    text += std::to_string(sample.droppedBound);
    text += " stagingWaitUs=";
    text += std::to_string(sample.stagingWaitUs);
    text += " trackingLagMs=";
    text += std::to_string(sample.trackingLagMs);
    text += " conf=";
    AppendDouble(text, "%.3f", sample.confidence);
    text += " inliers=";
    text += std::to_string(sample.inliers);
    text += " residualPx=";
    AppendDouble(text, "%.3f", sample.residualPx);
    text += " parity=";
    text += sample.parity.empty() ? "none" : sample.parity;
    text += " state=";
    text += sample.trackingState.empty() ? "Unattached" : sample.trackingState;
    text += " dpi=";
    text += std::to_string(sample.dpi);
    text += " roi=";
    text += std::to_string(sample.roiW);
    text += "x";
    text += std::to_string(sample.roiH);
    text += " affinityOk=";
    text += sample.affinityOk ? "yes" : "no";
    text += " deviceHr=";
    AppendHex32(text, sample.deviceHr);
    text += " removedHr=";
    AppendHex32(text, sample.deviceRemovedHr);
    return text;
}

void MetricsCollector::RecordSample(MetricsSample const& sample)
{
    samples_.push_back(sample);
    while (samples_.size() > kMaxMetricSamples)
    {
        samples_.pop_front();
    }
}

void MetricsCollector::RecordAffinity(bool affinityOk) noexcept
{
    if (!samples_.empty())
    {
        samples_.back().affinityOk = affinityOk;
    }
}

void MetricsCollector::SetObsVerification(ObsVerificationStatus status) noexcept
{
    obsVerify_ = status;
}

ObsVerificationStatus MetricsCollector::ObsVerification() const noexcept
{
    return obsVerify_;
}

void MetricsCollector::SetPreviewEnabled(bool enabled) noexcept
{
    previewEnabled_ = enabled;
}

void MetricsCollector::NoteReplayStatus(
    bool enabled,
    std::uint64_t bytes,
    std::uint64_t limit,
    bool stoppedSizeLimit) noexcept
{
    replayEnabled_ = enabled;
    replayBytes_ = bytes;
    replayLimit_ = limit;
    replayStoppedSizeLimit_ = stoppedSizeLimit;
}

void MetricsCollector::AppendEvent(std::string const& text)
{
    std::string sanitized = SanitizeLogText(text);
    if (sanitized.size() > kMaxLogEventChars)
    {
        sanitized.resize(kMaxLogEventChars);
    }
    events_.push_back(std::move(sanitized));
    while (events_.size() > kMaxLogEvents)
    {
        events_.pop_front();
    }
}

std::size_t MetricsCollector::SampleCount() const noexcept
{
    return samples_.size();
}

std::size_t MetricsCollector::EventCount() const noexcept
{
    return events_.size();
}

std::string const& MetricsCollector::OldestEvent() const
{
    if (events_.empty())
    {
        return kEmptyEvent;
    }
    return events_.front();
}

std::string const& MetricsCollector::NewestEvent() const
{
    if (events_.empty())
    {
        return kEmptyEvent;
    }
    return events_.back();
}

MetricsSummary MetricsCollector::Summarize() const
{
    MetricsSummary summary{};
    summary.sampleCount = samples_.size();
    summary.obsVerify = obsVerify_;
    summary.previewEnabled = previewEnabled_;
    summary.replayEnabled = replayEnabled_;
    summary.replayBytes = replayBytes_;
    summary.replayLimit = replayLimit_;
    summary.replayStoppedSizeLimit = replayStoppedSizeLimit_;
    if (samples_.empty())
    {
        return summary;
    }

    std::vector<double> captureAge;
    std::vector<double> stagingWait;
    std::vector<double> trackingLag;
    std::vector<double> confidence;
    std::vector<double> residual;
    captureAge.reserve(samples_.size());
    stagingWait.reserve(samples_.size());
    trackingLag.reserve(samples_.size());
    confidence.reserve(samples_.size());
    residual.reserve(samples_.size());
    for (MetricsSample const& sample : samples_)
    {
        captureAge.push_back(static_cast<double>(sample.captureAgeMs));
        stagingWait.push_back(static_cast<double>(sample.stagingWaitUs));
        trackingLag.push_back(static_cast<double>(sample.trackingLagMs));
        confidence.push_back(sample.confidence);
        residual.push_back(sample.residualPx);
    }
    summary.captureAgeMs = MakePercentileSummary(captureAge);
    summary.stagingWaitUs = MakePercentileSummary(stagingWait);
    summary.trackingLagMs = MakePercentileSummary(trackingLag);
    summary.confidence = MakePercentileSummary(confidence);
    summary.residualPx = MakePercentileSummary(residual);

    MetricsSample const& last = samples_.back();
    summary.lastDroppedBound = last.droppedBound;
    summary.lastDpi = last.dpi;
    summary.lastRoiW = last.roiW;
    summary.lastRoiH = last.roiH;
    summary.lastParity = last.parity;
    summary.lastTrackingState = last.trackingState;
    summary.lastAffinityOk = last.affinityOk;
    summary.lastDeviceHr = last.deviceHr;
    summary.lastDeviceRemovedHr = last.deviceRemovedHr;
    return summary;
}

std::string MetricsCollector::FormatSummary() const
{
    MetricsSummary const summary = Summarize();
    std::string text = "metrics samples=";
    text += std::to_string(summary.sampleCount);
    text += " captureAgeMs p50=";
    AppendDouble(text, "%.0f", summary.captureAgeMs.p50);
    text += " p95=";
    AppendDouble(text, "%.0f", summary.captureAgeMs.p95);
    text += " max=";
    AppendDouble(text, "%.0f", summary.captureAgeMs.max);
    text += " stagingUs p50=";
    AppendDouble(text, "%.0f", summary.stagingWaitUs.p50);
    text += " p95=";
    AppendDouble(text, "%.0f", summary.stagingWaitUs.p95);
    text += " max=";
    AppendDouble(text, "%.0f", summary.stagingWaitUs.max);
    text += " trackingLagMs p50=";
    AppendDouble(text, "%.0f", summary.trackingLagMs.p50);
    text += " p95=";
    AppendDouble(text, "%.0f", summary.trackingLagMs.p95);
    text += " max=";
    AppendDouble(text, "%.0f", summary.trackingLagMs.max);
    text += " conf p50=";
    AppendDouble(text, "%.3f", summary.confidence.p50);
    text += " p95=";
    AppendDouble(text, "%.3f", summary.confidence.p95);
    text += " max=";
    AppendDouble(text, "%.3f", summary.confidence.max);
    text += " residualPx p50=";
    AppendDouble(text, "%.3f", summary.residualPx.p50);
    text += " p95=";
    AppendDouble(text, "%.3f", summary.residualPx.p95);
    text += " max=";
    AppendDouble(text, "%.3f", summary.residualPx.max);
    text += " droppedBound=";
    text += std::to_string(summary.lastDroppedBound);
    text += " dpi=";
    text += std::to_string(summary.lastDpi);
    text += " roi=";
    text += std::to_string(summary.lastRoiW);
    text += "x";
    text += std::to_string(summary.lastRoiH);
    text += " state=";
    text += summary.lastTrackingState.empty() ? "Unattached" : summary.lastTrackingState;
    text += " parity=";
    text += summary.lastParity.empty() ? "none" : summary.lastParity;
    text += " affinityOk=";
    text += summary.lastAffinityOk ? "yes" : "no";
    text += " deviceHr=";
    AppendHex32(text, summary.lastDeviceHr);
    text += " removedHr=";
    AppendHex32(text, summary.lastDeviceRemovedHr);
    text += " obsVerify=";
    text += FormatObsVerificationStatus(summary.obsVerify);
    text += " (user-recorded; affinity API is not certification)";
    text += " preview=";
    text += summary.previewEnabled ? "on" : "off";
    text += " replay=";
    if (summary.replayStoppedSizeLimit)
    {
        text += "stopped-size-limit";
    }
    else if (!summary.replayEnabled)
    {
        text += "off";
    }
    else
    {
        text += std::to_string(summary.replayBytes);
        text += "/";
        text += std::to_string(summary.replayLimit);
    }
    text += " (default logs omit pixels/full paths/raw input)";
    return text;
}

ReplayRecorder::ReplayRecorder(std::uint64_t maxBytes, std::size_t maxFiles)
    : maxBytes_(maxBytes == 0 ? kMaxReplayBytes : maxBytes)
    , maxFiles_(maxFiles == 0 ? kMaxReplayFiles : maxFiles)
{
}

void ReplayRecorder::SetDirectory(std::string directory)
{
    directory_ = std::move(directory);
}

void ReplayRecorder::SetEnabled(bool enabled) noexcept
{
    enabled_ = enabled;
    if (!enabled_)
    {
        lastReject_ = ReplayReject::Disabled;
    }
}

bool ReplayRecorder::Enabled() const noexcept
{
    return enabled_;
}

std::uint64_t ReplayRecorder::BytesWritten() const noexcept
{
    return bytesWritten_;
}

std::uint64_t ReplayRecorder::MaxBytes() const noexcept
{
    return maxBytes_;
}

std::size_t ReplayRecorder::FilesWritten() const noexcept
{
    return filesWritten_;
}

bool ReplayRecorder::StoppedSizeLimit() const noexcept
{
    return stoppedSizeLimit_;
}

ReplayReject ReplayRecorder::LastReject() const noexcept
{
    return lastReject_;
}

ReplayReject ReplayRecorder::TryReserve(std::uint64_t bytes) noexcept
{
    if (!enabled_)
    {
        lastReject_ = ReplayReject::Disabled;
        return lastReject_;
    }
    if (directory_.empty())
    {
        lastReject_ = ReplayReject::EmptyDirectory;
        return lastReject_;
    }
    if (stoppedSizeLimit_ || filesWritten_ >= maxFiles_ ||
        bytesWritten_ + bytes > maxBytes_)
    {
        stoppedSizeLimit_ = true;
        lastReject_ = ReplayReject::SizeLimit;
        return lastReject_;
    }
    lastReject_ = ReplayReject::Ok;
    return lastReject_;
}

bool ReplayRecorder::WriteAll(std::string const& path, void const* data, std::size_t byteCount)
{
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out)
    {
        return false;
    }
    if (byteCount > 0 && data != nullptr)
    {
        out.write(static_cast<char const*>(data), static_cast<std::streamsize>(byteCount));
    }
    return static_cast<bool>(out);
}

ReplayReject ReplayRecorder::RecordNumericLine(std::string const& line)
{
    std::string const sanitized = SanitizeLogText(line) + "\n";
    ReplayReject const reserved = TryReserve(static_cast<std::uint64_t>(sanitized.size()));
    if (reserved != ReplayReject::Ok)
    {
        return reserved;
    }

    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec)
    {
        lastReject_ = ReplayReject::WriteFailed;
        return lastReject_;
    }

    std::string const path = (std::filesystem::path(directory_) / "samples.log").string();
    if (!WriteAll(path, sanitized.data(), sanitized.size()))
    {
        lastReject_ = ReplayReject::WriteFailed;
        return lastReject_;
    }
    bytesWritten_ += static_cast<std::uint64_t>(sanitized.size());
    if (filesWritten_ == 0)
    {
        ++filesWritten_;
    }
    lastReject_ = ReplayReject::Ok;
    return lastReject_;
}

ReplayReject ReplayRecorder::RecordImageFrame(
    std::uint64_t sequence,
    std::uint8_t const* bgra,
    std::size_t byteCount)
{
    ReplayReject const reserved = TryReserve(static_cast<std::uint64_t>(byteCount));
    if (reserved != ReplayReject::Ok)
    {
        return reserved;
    }
    if (byteCount > 0 && bgra == nullptr)
    {
        lastReject_ = ReplayReject::WriteFailed;
        return lastReject_;
    }

    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec)
    {
        lastReject_ = ReplayReject::WriteFailed;
        return lastReject_;
    }

    std::ostringstream name;
    name << "seq" << sequence << ".bgra";
    std::string const path = (std::filesystem::path(directory_) / name.str()).string();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        lastReject_ = ReplayReject::WriteFailed;
        return lastReject_;
    }
    if (byteCount > 0)
    {
        out.write(reinterpret_cast<char const*>(bgra), static_cast<std::streamsize>(byteCount));
    }
    if (!out)
    {
        lastReject_ = ReplayReject::WriteFailed;
        return lastReject_;
    }
    bytesWritten_ += static_cast<std::uint64_t>(byteCount);
    ++filesWritten_;
    lastReject_ = ReplayReject::Ok;
    return lastReject_;
}

std::string ReplayRecorder::FormatStatus() const
{
    if (stoppedSizeLimit_)
    {
        return "replay=stopped-size-limit";
    }
    if (!enabled_)
    {
        return "replay=off";
    }
    return "replay=" + std::to_string(bytesWritten_) + "/" + std::to_string(maxBytes_);
}

} // namespace tracing::diagnostics
