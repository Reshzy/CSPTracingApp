#include <gtest/gtest.h>

#include "platform/TargetDiscovery.h"

namespace {

using tracing::platform::CandidateKind;
using tracing::platform::CheckIdentity;
using tracing::platform::ClassifyCandidate;
using tracing::platform::FormatAccessFailure;
using tracing::platform::IdentityCheck;
using tracing::platform::IsLikelyCspLauncherExecutable;
using tracing::platform::IsLikelyCspPaintExecutable;
using tracing::platform::ResolveSelection;
using tracing::platform::SelectionStatus;
using tracing::platform::WindowCandidate;

HWND FakeHwnd(std::uintptr_t value)
{
    return reinterpret_cast<HWND>(value);
}

WindowCandidate MakeCandidate(
    std::uintptr_t hwndValue,
    std::uint32_t pid,
    std::uint64_t creationTime,
    std::wstring imageBaseName,
    CandidateKind kind)
{
    WindowCandidate candidate{};
    candidate.hwnd = FakeHwnd(hwndValue);
    candidate.process.pid = pid;
    candidate.process.creationTime = creationTime;
    candidate.process.imageBaseName = std::move(imageBaseName);
    candidate.visible = kind != CandidateKind::RejectedNotVisible;
    candidate.hasOwner = kind == CandidateKind::RejectedOwned;
    candidate.kind = kind;
    return candidate;
}

} // namespace

TEST(TargetClassification, PaintAndLauncherUseExecutableIdentity)
{
    EXPECT_TRUE(IsLikelyCspPaintExecutable(L"CLIPStudioPaint.exe"));
    EXPECT_TRUE(IsLikelyCspPaintExecutable(L"clipstudiopaint.exe"));
    EXPECT_FALSE(IsLikelyCspPaintExecutable(L"CLIPStudio.exe"));
    EXPECT_TRUE(IsLikelyCspLauncherExecutable(L"CLIPStudio.exe"));
    EXPECT_FALSE(IsLikelyCspLauncherExecutable(L"CLIPStudioPaint.exe"));
}

TEST(TargetClassification, IgnoresLocalizedTitleByUsingExeAndOwnerVisibility)
{
    EXPECT_EQ(
        ClassifyCandidate(L"CLIPStudioPaint.exe", true, false, true),
        CandidateKind::LikelyPaint);
    EXPECT_EQ(
        ClassifyCandidate(L"CLIPStudio.exe", true, false, true),
        CandidateKind::LikelyLauncher);
    EXPECT_EQ(
        ClassifyCandidate(L"CLIPStudioPaint.exe", false, false, true),
        CandidateKind::RejectedNotVisible);
    EXPECT_EQ(
        ClassifyCandidate(L"CLIPStudioPaint.exe", true, true, true),
        CandidateKind::RejectedOwned);
    EXPECT_EQ(ClassifyCandidate(L"notepad.exe", true, false, true), CandidateKind::OtherVisible);
    EXPECT_EQ(ClassifyCandidate(L"", true, false, false), CandidateKind::AccessFailed);
}

TEST(TargetSelection, AmbiguousPaintWindowsRequireExplicitChoice)
{
    std::vector<WindowCandidate> candidates;
    candidates.push_back(MakeCandidate(0x100, 11, 1001, L"CLIPStudioPaint.exe", CandidateKind::LikelyPaint));
    candidates.push_back(MakeCandidate(0x200, 22, 2002, L"CLIPStudioPaint.exe", CandidateKind::LikelyPaint));
    candidates.push_back(MakeCandidate(0x300, 33, 3003, L"CLIPStudio.exe", CandidateKind::LikelyLauncher));

    auto const ambiguous = ResolveSelection(candidates, std::nullopt, 7);
    EXPECT_EQ(ambiguous.status, SelectionStatus::Ambiguous);
    EXPECT_EQ(ambiguous.identity.hwnd, nullptr);

    auto const chosen = ResolveSelection(candidates, std::size_t{1}, 8);
    EXPECT_EQ(chosen.status, SelectionStatus::Selected);
    EXPECT_EQ(chosen.identity.hwnd, FakeHwnd(0x200));
    EXPECT_EQ(chosen.identity.process.pid, 22u);
    EXPECT_EQ(chosen.identity.process.creationTime, 2002u);
    EXPECT_EQ(chosen.identity.sessionGeneration, 8u);
}

TEST(TargetSelection, SinglePaintCanResolveWithoutIndexAndLauncherIsRejected)
{
    std::vector<WindowCandidate> candidates;
    candidates.push_back(MakeCandidate(0x10, 5, 55, L"CLIPStudio.exe", CandidateKind::LikelyLauncher));
    candidates.push_back(MakeCandidate(0x20, 6, 66, L"CLIPStudioPaint.exe", CandidateKind::LikelyPaint));

    auto const selected = ResolveSelection(candidates, std::nullopt, 3);
    EXPECT_EQ(selected.status, SelectionStatus::Selected);
    EXPECT_EQ(selected.identity.hwnd, FakeHwnd(0x20));
    EXPECT_EQ(selected.identity.sessionGeneration, 3u);

    auto const launcher = ResolveSelection(candidates, std::size_t{0}, 4);
    EXPECT_EQ(launcher.status, SelectionStatus::RejectedLauncher);
}

TEST(TargetSelection, RejectsDeadInvisibleAndUnknownProcesses)
{
    std::vector<WindowCandidate> candidates;
    candidates.push_back(
        MakeCandidate(0x1, 1, 1, L"CLIPStudioPaint.exe", CandidateKind::RejectedNotVisible));
    auto const dead = ResolveSelection(candidates, std::size_t{0}, 1);
    EXPECT_EQ(dead.status, SelectionStatus::RejectedDead);

    WindowCandidate access = MakeCandidate(0x2, 9, 0, L"", CandidateKind::AccessFailed);
    access.visible = true;
    access.accessError = FormatAccessFailure(9, ERROR_ACCESS_DENIED);
    candidates.push_back(access);
    auto const denied = ResolveSelection(candidates, std::size_t{1}, 1);
    EXPECT_EQ(denied.status, SelectionStatus::AccessFailed);
    EXPECT_NE(denied.message.find(L"Admin rights are not the default fix"), std::wstring::npos);

    WindowCandidate other = MakeCandidate(0x3, 8, 8, L"notepad.exe", CandidateKind::OtherVisible);
    candidates.push_back(other);
    auto const rejectedOther = ResolveSelection(candidates, std::size_t{2}, 1);
    EXPECT_EQ(rejectedOther.status, SelectionStatus::RejectedOther);
}

TEST(TargetIdentity, DetectsDeadWindowAndHandleReuse)
{
    EXPECT_EQ(CheckIdentity(10, 100, false, false, 0, 0), IdentityCheck::WindowDead);
    EXPECT_EQ(CheckIdentity(10, 100, true, true, 10, 100), IdentityCheck::Match);
    EXPECT_EQ(CheckIdentity(10, 100, true, true, 99, 100), IdentityCheck::HandleReused);
    EXPECT_EQ(CheckIdentity(10, 100, true, true, 10, 999), IdentityCheck::HandleReused);
    EXPECT_EQ(CheckIdentity(10, 100, true, false, 10, 100), IdentityCheck::AccessFailed);
}
