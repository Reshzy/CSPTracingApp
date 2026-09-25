#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/DeviceResources.h"

#include <cstdint>
#include <memory>
#include <string>

namespace tracing::capture {

enum class CaptureSessionState
{
    Idle,
    Unsupported,
    Starting,
    Running,
    ItemClosed,
    Failed,
    Stopped,
};

enum class CaptureSessionEvent
{
    SupportPresent,
    SupportMissing,
    StartSucceeded,
    StartFailed,
    FrameAccepted,
    FrameDroppedBound,
    FrameRejectedGeneration,
    FrameStaleZeroSize,
    ItemClosed,
    Stop,
};

enum class CaptureHandoffAction
{
    RejectNotRunning,
    RejectWrongGeneration,
    StaleZeroSize,
    DropOldestThenEnqueue,
    Enqueue,
};

struct CaptureFrameArrival
{
    std::uint64_t targetGeneration = 0;
    int contentWidth = 0;
    int contentHeight = 0;
};

struct CaptureHandoffResult
{
    CaptureHandoffAction action = CaptureHandoffAction::RejectNotRunning;
    std::uint64_t sequence = 0;
};

struct FramePacket
{
    std::uint64_t sequence = 0;
    std::int64_t captureTicks = 0;
    int contentWidth = 0;
    int contentHeight = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    bool stale = false;
};

// GPU-free lifecycle and bounded handoff policy. Missing WGC support is
// Unsupported, never Failed. Sequence increments only on accepted frames.
class CaptureSessionPolicy
{
public:
    static constexpr std::size_t kMaxPending = 2;

    CaptureSessionState Apply(CaptureSessionEvent event) noexcept;
    CaptureSessionState State() const noexcept;

    void SetSessionGeneration(std::uint64_t generation) noexcept;
    std::uint64_t SessionGeneration() const noexcept;
    bool GenerationValid() const noexcept;

    CaptureHandoffResult Arrive(CaptureFrameArrival const& arrival) noexcept;
    void NoteDequeued() noexcept;

    std::uint64_t Accepted() const noexcept;
    std::uint64_t DroppedBound() const noexcept;
    std::uint64_t RejectedGeneration() const noexcept;
    std::uint64_t Stale() const noexcept;
    std::size_t Pending() const noexcept;
    std::uint64_t LastSequence() const noexcept;

private:
    CaptureSessionState state_ = CaptureSessionState::Idle;
    std::uint64_t sessionGeneration_ = 0;
    bool generationValid_ = false;
    std::size_t pending_ = 0;
    std::uint64_t accepted_ = 0;
    std::uint64_t droppedBound_ = 0;
    std::uint64_t rejectedGeneration_ = 0;
    std::uint64_t stale_ = 0;
    std::uint64_t lastSequence_ = 0;
};

std::wstring FormatCaptureSessionState(CaptureSessionState state);
std::wstring FormatCaptureHandoffAction(CaptureHandoffAction action);

// WGC session for a borrowed HWND and borrowed D3D device. FrameArrived never
// uses the immediate context. Owned copies happen on the graphics/UI thread.
class CaptureSession
{
public:
    CaptureSession();
    ~CaptureSession();

    CaptureSession(CaptureSession const&) = delete;
    CaptureSession& operator=(CaptureSession const&) = delete;
    CaptureSession(CaptureSession&&) = delete;
    CaptureSession& operator=(CaptureSession&&) = delete;

    bool Start(
        HWND target,
        std::uint64_t targetGeneration,
        std::uint64_t geometryGeneration,
        graphics::DeviceResources& device,
        HWND notifyWindow,
        UINT notifyMessage,
        std::wstring& error);
    void Stop();
    void OnItemClosed();
    void PumpHandoff();

    CaptureSessionState State() const noexcept;
    FramePacket LastPacket() const;
    bool HasOwnedFrame() const noexcept;
    std::wstring FormatReport() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

inline CaptureSessionState CaptureSessionPolicy::State() const noexcept
{
    return state_;
}

inline std::uint64_t CaptureSessionPolicy::SessionGeneration() const noexcept
{
    return sessionGeneration_;
}

inline bool CaptureSessionPolicy::GenerationValid() const noexcept
{
    return generationValid_;
}

inline std::uint64_t CaptureSessionPolicy::Accepted() const noexcept
{
    return accepted_;
}

inline std::uint64_t CaptureSessionPolicy::DroppedBound() const noexcept
{
    return droppedBound_;
}

inline std::uint64_t CaptureSessionPolicy::RejectedGeneration() const noexcept
{
    return rejectedGeneration_;
}

inline std::uint64_t CaptureSessionPolicy::Stale() const noexcept
{
    return stale_;
}

inline std::size_t CaptureSessionPolicy::Pending() const noexcept
{
    return pending_;
}

inline std::uint64_t CaptureSessionPolicy::LastSequence() const noexcept
{
    return lastSequence_;
}

inline void CaptureSessionPolicy::SetSessionGeneration(std::uint64_t generation) noexcept
{
    sessionGeneration_ = generation;
    generationValid_ = true;
}

inline void CaptureSessionPolicy::NoteDequeued() noexcept
{
    if (pending_ > 0)
    {
        --pending_;
    }
}

inline CaptureSessionState CaptureSessionPolicy::Apply(CaptureSessionEvent event) noexcept
{
    if (event == CaptureSessionEvent::Stop)
    {
        state_ = CaptureSessionState::Stopped;
        generationValid_ = false;
        pending_ = 0;
        return state_;
    }

    if (event == CaptureSessionEvent::SupportMissing)
    {
        state_ = CaptureSessionState::Unsupported;
        generationValid_ = false;
        return state_;
    }

    if (event == CaptureSessionEvent::ItemClosed)
    {
        if (state_ == CaptureSessionState::Running || state_ == CaptureSessionState::Starting)
        {
            state_ = CaptureSessionState::ItemClosed;
        }
        generationValid_ = false;
        pending_ = 0;
        return state_;
    }

    switch (state_)
    {
    case CaptureSessionState::Idle:
    case CaptureSessionState::Stopped:
        if (event == CaptureSessionEvent::SupportPresent)
        {
            state_ = CaptureSessionState::Starting;
        }
        else if (event == CaptureSessionEvent::StartSucceeded)
        {
            state_ = CaptureSessionState::Running;
        }
        else if (event == CaptureSessionEvent::StartFailed)
        {
            state_ = CaptureSessionState::Failed;
        }
        break;

    case CaptureSessionState::Starting:
        if (event == CaptureSessionEvent::StartSucceeded)
        {
            state_ = CaptureSessionState::Running;
        }
        else if (event == CaptureSessionEvent::StartFailed)
        {
            state_ = CaptureSessionState::Failed;
        }
        break;

    case CaptureSessionState::Running:
        if (event == CaptureSessionEvent::StartFailed)
        {
            state_ = CaptureSessionState::Failed;
            generationValid_ = false;
        }
        break;

    case CaptureSessionState::Unsupported:
        if (event == CaptureSessionEvent::SupportPresent)
        {
            state_ = CaptureSessionState::Starting;
        }
        else if (event == CaptureSessionEvent::StartSucceeded)
        {
            state_ = CaptureSessionState::Running;
        }
        else if (event == CaptureSessionEvent::StartFailed)
        {
            state_ = CaptureSessionState::Failed;
        }
        break;

    case CaptureSessionState::ItemClosed:
    case CaptureSessionState::Failed:
        if (event == CaptureSessionEvent::SupportPresent)
        {
            state_ = CaptureSessionState::Starting;
        }
        else if (event == CaptureSessionEvent::StartSucceeded)
        {
            state_ = CaptureSessionState::Running;
        }
        break;
    }

    return state_;
}

inline CaptureHandoffResult CaptureSessionPolicy::Arrive(CaptureFrameArrival const& arrival) noexcept
{
    CaptureHandoffResult result{};
    if (state_ != CaptureSessionState::Running || !generationValid_)
    {
        result.action = CaptureHandoffAction::RejectNotRunning;
        return result;
    }

    if (arrival.targetGeneration != sessionGeneration_)
    {
        ++rejectedGeneration_;
        result.action = CaptureHandoffAction::RejectWrongGeneration;
        return result;
    }

    if (arrival.contentWidth <= 0 || arrival.contentHeight <= 0)
    {
        ++stale_;
        result.action = CaptureHandoffAction::StaleZeroSize;
        return result;
    }

    ++lastSequence_;
    ++accepted_;
    result.sequence = lastSequence_;
    if (pending_ >= kMaxPending)
    {
        ++droppedBound_;
        result.action = CaptureHandoffAction::DropOldestThenEnqueue;
        return result;
    }

    ++pending_;
    result.action = CaptureHandoffAction::Enqueue;
    return result;
}

inline std::wstring FormatCaptureSessionState(CaptureSessionState state)
{
    switch (state)
    {
    case CaptureSessionState::Idle:
        return L"Idle";
    case CaptureSessionState::Unsupported:
        return L"Unsupported";
    case CaptureSessionState::Starting:
        return L"Starting";
    case CaptureSessionState::Running:
        return L"Running";
    case CaptureSessionState::ItemClosed:
        return L"ItemClosed";
    case CaptureSessionState::Failed:
        return L"Failed";
    case CaptureSessionState::Stopped:
        return L"Stopped";
    }
    return L"Unknown";
}

inline std::wstring FormatCaptureHandoffAction(CaptureHandoffAction action)
{
    switch (action)
    {
    case CaptureHandoffAction::RejectNotRunning:
        return L"reject-not-running";
    case CaptureHandoffAction::RejectWrongGeneration:
        return L"reject-wrong-generation";
    case CaptureHandoffAction::StaleZeroSize:
        return L"stale-zero-size";
    case CaptureHandoffAction::DropOldestThenEnqueue:
        return L"drop-oldest-then-enqueue";
    case CaptureHandoffAction::Enqueue:
        return L"enqueue";
    }
    return L"unknown";
}

} // namespace tracing::capture
