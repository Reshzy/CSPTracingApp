#include <gtest/gtest.h>

#include "diagnostics/Metrics.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using tracing::diagnostics::FormatMetricsSampleLine;
using tracing::diagnostics::FormatObsVerificationStatus;
using tracing::diagnostics::FormatReplayReject;
using tracing::diagnostics::kMaxLogEvents;
using tracing::diagnostics::kMaxMetricSamples;
using tracing::diagnostics::MetricsCollector;
using tracing::diagnostics::MetricsSample;
using tracing::diagnostics::ObsVerificationStatus;
using tracing::diagnostics::ParseU64Field;
using tracing::diagnostics::Percentile;
using tracing::diagnostics::ReplayRecorder;
using tracing::diagnostics::ReplayReject;
using tracing::diagnostics::SanitizeLogText;

MetricsSample AgeSample(std::int64_t ageMs)
{
    MetricsSample sample{};
    sample.captureAgeMs = ageMs;
    sample.trackingState = "Tracking";
    sample.parity = "none";
    return sample;
}

std::filesystem::path MakeTempDir()
{
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "tracingapp-metrics-tests";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace

TEST(Percentile, EmptyReturnsZero)
{
    EXPECT_DOUBLE_EQ(Percentile({}, 0.50), 0.0);
    EXPECT_DOUBLE_EQ(Percentile({}, 0.95), 0.0);
    EXPECT_DOUBLE_EQ(Percentile({}, 1.00), 0.0);
}

TEST(Percentile, ZeroToNinetyNineNearestRank)
{
    std::vector<double> values;
    values.reserve(100);
    for (int i = 0; i < 100; ++i)
    {
        values.push_back(static_cast<double>(i));
    }
    EXPECT_DOUBLE_EQ(Percentile(values, 0.50), 49.0);
    EXPECT_DOUBLE_EQ(Percentile(values, 0.95), 94.0);
    EXPECT_DOUBLE_EQ(Percentile(values, 1.00), 99.0);
}

TEST(MetricsCollector, SampleWindowDoesNotGrowPast256)
{
    MetricsCollector collector;
    for (int i = 0; i < 300; ++i)
    {
        collector.RecordSample(AgeSample(i));
    }
    EXPECT_EQ(collector.SampleCount(), kMaxMetricSamples);
    auto const summary = collector.Summarize();
    EXPECT_EQ(summary.sampleCount, kMaxMetricSamples);
    EXPECT_DOUBLE_EQ(summary.captureAgeMs.max, 299.0);
    EXPECT_DOUBLE_EQ(summary.captureAgeMs.p50, 171.0);
}

TEST(MetricsCollector, EventLogDropsOldestAt128)
{
    MetricsCollector collector;
    for (int i = 0; i < 200; ++i)
    {
        collector.AppendEvent("event-" + std::to_string(i));
    }
    EXPECT_EQ(collector.EventCount(), kMaxLogEvents);
    EXPECT_EQ(collector.OldestEvent(), "event-72");
    EXPECT_EQ(collector.NewestEvent(), "event-199");
}

TEST(SanitizeLogText, StripsFullPrivatePathToLeaf)
{
    std::string const sanitized =
        SanitizeLogText("loaded I:\\Documents\\secret\\art.png ok");
    EXPECT_NE(sanitized.find("art.png"), std::string::npos);
    EXPECT_EQ(sanitized.find("Documents"), std::string::npos);
    EXPECT_EQ(sanitized.find("secret"), std::string::npos);
    EXPECT_EQ(sanitized.find("I:"), std::string::npos);
}

TEST(SanitizeLogText, OmitsPixelDumpAndRawInputPayload)
{
    std::string const pixels =
        SanitizeLogText("bgra=" + std::string(40, 'A') + std::string(40, 'B'));
    EXPECT_NE(pixels.find("<pixels>"), std::string::npos);
    EXPECT_EQ(pixels.find("AAAAAAAA"), std::string::npos);

    std::string const raw = SanitizeLogText(
        "rawInput bytes=00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF");
    EXPECT_NE(raw.find("<raw-input>"), std::string::npos);
    EXPECT_EQ(raw.find("00112233"), std::string::npos);
}

TEST(MetricsCollector, AffinityOkDoesNotCertifyObs)
{
    MetricsCollector collector;
    MetricsSample sample{};
    sample.affinityOk = true;
    sample.trackingState = "Tracking";
    collector.RecordSample(sample);
    collector.RecordAffinity(true);
    EXPECT_EQ(collector.ObsVerification(), ObsVerificationStatus::NotRun);
    std::string const text = collector.FormatSummary();
    EXPECT_NE(text.find("affinityOk=yes"), std::string::npos);
    EXPECT_NE(text.find("obsVerify=NOT RUN"), std::string::npos);
    EXPECT_NE(text.find("user-recorded; affinity API is not certification"), std::string::npos);
    EXPECT_EQ(text.find("user-PASS"), std::string::npos);

    collector.SetObsVerification(ObsVerificationStatus::UserPass);
    EXPECT_EQ(collector.ObsVerification(), ObsVerificationStatus::UserPass);
    EXPECT_NE(collector.FormatSummary().find("obsVerify=user-PASS"), std::string::npos);
}

TEST(ReplayRecorder, DefaultDisabledWritesZeroBytes)
{
    ReplayRecorder recorder;
    EXPECT_FALSE(recorder.Enabled());
    EXPECT_EQ(recorder.RecordNumericLine("captureAgeMs=1"), ReplayReject::Disabled);
    EXPECT_EQ(recorder.BytesWritten(), 0u);
    EXPECT_EQ(recorder.FormatStatus(), "replay=off");
    EXPECT_STREQ(FormatReplayReject(ReplayReject::Disabled), "disabled");
}

TEST(ReplayRecorder, OptInStopsAtInjectedSizeLimit)
{
    std::filesystem::path const dir = MakeTempDir();
    ReplayRecorder recorder(100, 8);
    recorder.SetDirectory(dir.string());
    recorder.SetEnabled(true);
    ReplayReject const first = recorder.RecordNumericLine(std::string(60, 'z'));
    EXPECT_EQ(first, ReplayReject::Ok);
    EXPECT_GT(recorder.BytesWritten(), 0u);
    EXPECT_LT(recorder.BytesWritten(), 100u);

    ReplayReject const second = recorder.RecordNumericLine(std::string(50, 'z'));
    EXPECT_EQ(second, ReplayReject::SizeLimit);
    EXPECT_TRUE(recorder.StoppedSizeLimit());
    EXPECT_EQ(recorder.FormatStatus(), "replay=stopped-size-limit");
    EXPECT_LE(recorder.BytesWritten(), 100u);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(ParseU64Field, ReadsDroppedBoundAndFirstMapWait)
{
    std::wstring const report =
        L"accepted=12 droppedBound=7 staleZero=0\r\n"
        L"roi kind=canvas copyUs=3 mapWaitUs=11 pending=1 dropped=0\r\n"
        L"nav mapWaitUs=99";
    std::uint64_t dropped = 0;
    std::uint64_t wait = 0;
    EXPECT_TRUE(ParseU64Field(report, L"droppedBound=", dropped));
    EXPECT_EQ(dropped, 7u);
    EXPECT_TRUE(ParseU64Field(report, L"mapWaitUs=", wait));
    EXPECT_EQ(wait, 11u);
}

TEST(FormatMetricsSampleLine, NumericOnlyNoPaths)
{
    MetricsSample sample{};
    sample.captureAgeMs = 12;
    sample.droppedBound = 2;
    sample.confidence = 0.5;
    sample.parity = "FlipX";
    sample.trackingState = "Degraded";
    std::string const line = FormatMetricsSampleLine(sample);
    EXPECT_NE(line.find("captureAgeMs=12"), std::string::npos);
    EXPECT_EQ(line.find("C:"), std::string::npos);
    EXPECT_EQ(line.find("bgra"), std::string::npos);
    EXPECT_STREQ(FormatObsVerificationStatus(ObsVerificationStatus::NotRun), "NOT RUN");
}
