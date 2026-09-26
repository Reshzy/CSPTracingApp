#pragma once

#include "core/Transform2D.h"

#include <cstdint>
#include <vector>

namespace tracing::replay {

// Seeded synthetic canvas sequences. Ground-truth warps and landmarks are
// independent of VisualTracker. Not private artwork and not real CSP frames.

inline constexpr int kReplayWidth = 320;
inline constexpr int kReplayHeight = 240;
inline constexpr std::uint32_t kReplayDefaultSeed = 0xC0FFu;

enum class ReplayEvent
{
    Keyframe,
    Pan,
    Scale,
    Rotate,
    Combined,
    Stroke,
    Missing,
    GapPan,
    Blank,
    Reacquire,
    FlipX,
    Identity,
};

struct ReplayFrame
{
    std::uint64_t sequence = 0;
    ReplayEvent event = ReplayEvent::Keyframe;
    bool missing = false;
    bool blank = false;
    bool stroke = false;
    bool flipX = false;
    bool flipY = false;
    bool settledMaster = false;
    double tx = 0.0;
    double ty = 0.0;
    double uniformScale = 1.0;
    double radiansClockwise = 0.0;
    core::Transform2D warp = core::Identity(core::Space::C, core::Space::C);
    int width = kReplayWidth;
    int height = kReplayHeight;
    int stride = kReplayWidth * 4;
    std::vector<std::uint8_t> bgra;
};

char const* ReplayEventName(ReplayEvent event) noexcept;

std::vector<core::Vec2> ReplayLandmarks();

std::vector<ReplayFrame> BuildSequence(std::uint32_t seed = kReplayDefaultSeed);

} // namespace tracing::replay
