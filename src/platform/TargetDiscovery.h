#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tracing::platform {

enum class CandidateKind
{
    RejectedNotVisible,
    RejectedOwned,
    AccessFailed,
    OtherVisible,
    LikelyLauncher,
    LikelyPaint,
};

enum class SelectionStatus
{
    Selected,
    Ambiguous,
    NoPaintCandidate,
    InvalidIndex,
    RejectedDead,
    RejectedLauncher,
    RejectedOther,
    AccessFailed,
};

enum class IdentityCheck
{
    Match,
    WindowDead,
    HandleReused,
    AccessFailed,
};

struct ProcessIdentity
{
    std::uint32_t pid = 0;
    std::uint64_t creationTime = 0;
    std::wstring imagePath;
    std::wstring imageBaseName;
};

struct TargetIdentity
{
    HWND hwnd = nullptr;
    ProcessIdentity process;
    std::uint64_t sessionGeneration = 0;
};

struct WindowCandidate
{
    HWND hwnd = nullptr;
    ProcessIdentity process;
    bool visible = false;
    bool hasOwner = false;
    std::wstring title;
    std::wstring className;
    std::wstring accessError;
    CandidateKind kind = CandidateKind::RejectedNotVisible;
};

struct TargetSelection
{
    SelectionStatus status = SelectionStatus::NoPaintCandidate;
    TargetIdentity identity{};
    std::wstring message;
};

// Classification uses process image identity, visibility, and owner. Title and class are not inputs.
CandidateKind ClassifyCandidate(
    std::wstring_view imageBaseName,
    bool visible,
    bool hasOwner,
    bool processQuerySucceeded);

bool IsLikelyCspPaintExecutable(std::wstring_view imageBaseName);
bool IsLikelyCspLauncherExecutable(std::wstring_view imageBaseName);

IdentityCheck CheckIdentity(
    std::uint32_t storedPid,
    std::uint64_t storedCreationTime,
    bool hwndAlive,
    bool observedProcessKnown,
    std::uint32_t observedPid,
    std::uint64_t observedCreationTime);

TargetSelection ResolveSelection(
    std::span<WindowCandidate const> candidates,
    std::optional<std::size_t> userIndex,
    std::uint64_t sessionGeneration);

std::wstring FormatAccessFailure(std::uint32_t pid, unsigned long win32Error);
std::wstring FormatCandidateLine(WindowCandidate const& candidate);

// Enumerates top-level HWNDs. Call from the UI thread. Does not inject or request elevation.
bool EnumerateTopLevelCandidates(std::vector<WindowCandidate>& candidates, std::wstring& error);

} // namespace tracing::platform
