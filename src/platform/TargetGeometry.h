#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "platform/TargetDiscovery.h"

#include <cstdint>
#include <string>

namespace tracing::platform {

struct PhysicalRect
{
    long x = 0;
    long y = 0;
    long width = 0;
    long height = 0;
};

enum class OverlayEligibility
{
    Eligible,
    Minimized,
    Cloaked,
    NotForeground,
    InvalidGeometry,
    TargetDead,
    IdentityMismatch,
};

struct GeometrySnapshot
{
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    PhysicalRect clientPhysical{};
    PhysicalRect outerWindow{};
    PhysicalRect dwmFrame{};
    unsigned dpi = 0;
    bool hwndValid = false;
    bool visible = false;
    bool minimized = false;
    bool cloaked = false;
    bool targetForeground = false;
    bool identityValid = false;
    IdentityCheck identity = IdentityCheck::WindowDead;
};

// Client origin/size and DPI only. Outer/DWM frame changes are not canvas motion.
bool PhysicalGeometryChanged(GeometrySnapshot const& previous, GeometrySnapshot const& next) noexcept;

std::uint64_t NextGeometryGeneration(
    GeometrySnapshot const& previous,
    GeometrySnapshot const& sampled) noexcept;

OverlayEligibility ClassifyOverlayEligibility(GeometrySnapshot const& snapshot) noexcept;

std::wstring FormatPhysicalRect(PhysicalRect const& rect);
std::wstring FormatOverlayEligibility(OverlayEligibility eligibility);
std::wstring FormatGeometryReport(GeometrySnapshot const& snapshot);

// Re-validates HWND + process identity before reading geometry. Does not treat
// client bounds as canvas bounds. Canvas ROI is not part of this snapshot.
bool QueryPhysicalGeometry(
    TargetIdentity const& identity,
    GeometrySnapshot& snapshot,
    std::wstring& error);

// Out-of-context WinEvent hook. The callback only posts a message; it never
// samples geometry. WINEVENT_INCONTEXT is not used (no injection).
class TargetLifecycleWatcher
{
public:
    TargetLifecycleWatcher() = default;
    ~TargetLifecycleWatcher();

    TargetLifecycleWatcher(TargetLifecycleWatcher const&) = delete;
    TargetLifecycleWatcher& operator=(TargetLifecycleWatcher const&) = delete;
    TargetLifecycleWatcher(TargetLifecycleWatcher&&) = delete;
    TargetLifecycleWatcher& operator=(TargetLifecycleWatcher&&) = delete;

    bool Attach(HWND controlWindow, HWND targetWindow, unsigned notifyMessage, std::wstring& error);
    void Detach() noexcept;
    bool IsAttached() const noexcept;

private:
    void ResetHooks() noexcept;

    HWINEVENTHOOK systemHook_ = nullptr;
    HWINEVENTHOOK objectHook_ = nullptr;
};

} // namespace tracing::platform
