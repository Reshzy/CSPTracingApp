#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <commctrl.h>

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

bool IsValidControlViewport(int width, int height) noexcept
{
    return width > 0 && height > 0;
}

#ifndef TRACING_APP_TESTING

#include <Unknwn.h>
#include <winrt/base.h>

#include <shobjidl.h>
#include <wrl/client.h>

#include "capture/CaptureSession.h"
#include "graphics/CapturePreview.h"
#include "graphics/DeviceResources.h"
#include "graphics/ImageRenderer.h"
#include "graphics/OverlaySurface.h"
#include "core/Calibration.h"
#include "core/Transform2D.h"
#include "image/ImageLoader.h"
#include "platform/TargetDiscovery.h"
#include "platform/TargetGeometry.h"
#include "app/ReferenceWindow.h"
#include "tracking/TrackingSession.h"

namespace {
constexpr int kDefaultWidth = 720;
constexpr int kDefaultHeight = 980;
constexpr wchar_t kWindowClass[] = L"TracingAppControlWindow";
constexpr wchar_t kWindowTitle[] = L"TracingApp";
constexpr int kIdList = 1001;
constexpr int kIdRefresh = 1002;
constexpr int kIdSelect = 1003;
constexpr int kIdStatus = 1004;
constexpr int kIdEmergencyHide = 1005;
constexpr int kIdShowMarker = 1006;
constexpr int kIdTracingMode = 1007;
constexpr int kIdAlignmentMode = 1008;
constexpr int kIdCoverProbe = 1009;
constexpr int kIdStartCapture = 1010;
constexpr int kIdStopCapture = 1011;
constexpr int kIdEnablePreview = 1012;
constexpr int kIdImportImage = 1013;
constexpr int kIdOpacity = 1014;
constexpr int kIdFit = 1015;
constexpr int kIdResetPlacement = 1016;
constexpr int kIdShowReference = 1017;
constexpr int kIdObsPositiveControl = 1018;
constexpr int kIdObsSkipOverlayImage = 1019;
constexpr int kIdRecreateOverlay = 1020;
constexpr int kIdTransformDiag = 1021;
constexpr int kIdRoiX = 1022;
constexpr int kIdRoiY = 1023;
constexpr int kIdRoiW = 1024;
constexpr int kIdRoiH = 1025;
constexpr int kIdApplyRoi = 1026;
constexpr int kIdDocW = 1027;
constexpr int kIdDocH = 1028;
constexpr int kIdAlignN = 1029;
constexpr int kIdAlignS = 1030;
constexpr int kIdAlignW = 1031;
constexpr int kIdAlignE = 1032;
constexpr int kIdAlignScaleDown = 1033;
constexpr int kIdAlignScaleUp = 1034;
constexpr int kIdAlignRotLeft = 1035;
constexpr int kIdAlignRotRight = 1036;
constexpr int kIdAlignFlipX = 1037;
constexpr int kIdAlignFlipY = 1038;
constexpr int kIdCanvasN = 1039;
constexpr int kIdCanvasS = 1040;
constexpr int kIdCanvasW = 1041;
constexpr int kIdCanvasE = 1042;
constexpr int kIdCanvasZoomDown = 1043;
constexpr int kIdCanvasZoomUp = 1044;
constexpr int kIdCanvasRotLeft = 1045;
constexpr int kIdCanvasRotRight = 1046;
constexpr int kIdCanvasFlipX = 1047;
constexpr int kIdCanvasFlipY = 1048;
constexpr int kIdStartTracking = 1049;
constexpr int kIdPauseTracking = 1050;
constexpr int kIdResyncTracking = 1051;
constexpr UINT kMsgGeometry = WM_APP + 1;
constexpr UINT kMsgCapture = WM_APP + 2;
constexpr UINT kMsgStartCapture = WM_APP + 3;
constexpr UINT kMsgStopCapture = WM_APP + 4;
constexpr UINT_PTR kTimerGeometry = 1;
constexpr UINT kGeometryPollMs = 250;
constexpr double kAlignNudgePx = 8.0;
constexpr double kCanvasNudgePx = 16.0;
constexpr double kScaleStep = 1.05;
constexpr double kRotateStepRadians = 15.0 * std::numbers::pi_v<double> / 180.0;
constexpr double kMinZoom = 1.0e-6;
constexpr double kMaxZoom = 1.0e6;

struct LiveCalibration
{
    bool roiApplied = false;
    tracing::core::Rect2 roi{};
    std::uint64_t targetGeneration = 0;
    double clientWidth = 0.0;
    double clientHeight = 0.0;
    double declaredDocW = 0.0;
    double declaredDocH = 0.0;
    tracing::core::ReferenceAlignment mRd{};
    tracing::core::Vec2 documentAnchor{};
    double zoom = 1.0;
    double canvasRadians = 0.0;
    bool canvasFlipX = false;
    bool canvasFlipY = false;
    tracing::core::CalibrationInvalidation lastInvalidation =
        tracing::core::CalibrationInvalidation::None;
    bool axisAlignedPlacement = true;
    std::uint64_t calibrationGeneration = 0;
};

struct ControlState
{
    HWND control = nullptr;
    HWND list = nullptr;
    HWND status = nullptr;
    std::vector<tracing::platform::WindowCandidate> candidates;
    std::optional<tracing::platform::TargetIdentity> selected;
    tracing::platform::TargetLifecycleWatcher watcher;
    tracing::platform::GeometrySnapshot lastGeometry;
    tracing::graphics::DeviceResources device;
    tracing::graphics::OverlaySurface overlay;
    tracing::graphics::CapturePreview preview;
    tracing::capture::CaptureSession capture;
    tracing::image::LoadedImageSlot image;
    tracing::graphics::ImageRenderer renderer;
    tracing::app::ReferenceWindow reference;
    tracing::tracking::TrackingSession tracking;
    HWND previewCheck = nullptr;
    HWND opacityTrack = nullptr;
    HWND obsPositiveControlCheck = nullptr;
    HWND obsSkipOverlayImageCheck = nullptr;
    HWND roiXEdit = nullptr;
    HWND roiYEdit = nullptr;
    HWND roiWEdit = nullptr;
    HWND roiHEdit = nullptr;
    HWND docWEdit = nullptr;
    HWND docHEdit = nullptr;
    LiveCalibration calibration;
    bool hideOverlayOnCaptureLoss = false;
    bool obsSkipOverlayImagePresent = false;
    std::uint64_t nextGeneration = 1;
    std::uint64_t lastFedRoiSequence = 0;
    bool lastRoiFeedValid = false;
    tracing::tracking::TrackingFrameAction lastRoiFeedAction =
        tracing::tracking::TrackingFrameAction::RejectNotCalibrated;
};

std::wstring ControlDpiLine(HWND hwnd)
{
    unsigned const dpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : 0;
    return L"control DPI=" + std::to_wstring(dpi) +
           L" (physical, PerMonitorV2; not canvas bounds)\r\n";
}

tracing::graphics::CaptureToClientMapping MappingFromState(ControlState const& state)
{
    tracing::capture::FramePacket const packet = state.capture.LastPacket();
    tracing::platform::PhysicalRect const& client = state.lastGeometry.clientPhysical;
    tracing::platform::PhysicalRect const& outer = state.lastGeometry.outerWindow;
    tracing::platform::PhysicalRect const& dwm = state.lastGeometry.dwmFrame;
    return tracing::graphics::MeasureCaptureToClientMapping(
        packet.contentWidth,
        packet.contentHeight,
        client.x,
        client.y,
        client.width,
        client.height,
        outer.x,
        outer.y,
        outer.width,
        outer.height,
        dwm.x,
        dwm.y,
        dwm.width,
        dwm.height);
}

std::wstring WidenAscii(std::string const& ascii)
{
    return std::wstring(ascii.begin(), ascii.end());
}

tracing::core::CaptureMappingMatch CoreMappingMatch(
    tracing::graphics::CaptureSizeMatch match) noexcept
{
    switch (match)
    {
    case tracing::graphics::CaptureSizeMatch::OuterWindow:
        return tracing::core::CaptureMappingMatch::OuterWindow;
    case tracing::graphics::CaptureSizeMatch::DwmFrame:
        return tracing::core::CaptureMappingMatch::DwmFrame;
    case tracing::graphics::CaptureSizeMatch::OuterAndDwm:
        return tracing::core::CaptureMappingMatch::OuterAndDwm;
    default:
        return tracing::core::CaptureMappingMatch::None;
    }
}

tracing::core::CaptureMappingInput CoreMappingFromState(ControlState const& state)
{
    tracing::graphics::CaptureToClientMapping const mapping = MappingFromState(state);
    tracing::core::CaptureMappingInput input{};
    input.contentWidth = static_cast<double>(mapping.contentWidth);
    input.contentHeight = static_cast<double>(mapping.contentHeight);
    input.clientOriginS = tracing::core::Vec2{
        static_cast<double>(mapping.clientX),
        static_cast<double>(mapping.clientY)};
    input.clientWidth = static_cast<double>(mapping.clientWidth);
    input.clientHeight = static_cast<double>(mapping.clientHeight);
    input.captureToClient = tracing::core::Vec2{
        static_cast<double>(mapping.captureToClientX),
        static_cast<double>(mapping.captureToClientY)};
    input.match = CoreMappingMatch(mapping.sizeMatch);
    return input;
}

bool CalibrationIsLive(ControlState const& state) noexcept
{
    return state.calibration.roiApplied &&
           state.calibration.lastInvalidation == tracing::core::CalibrationInvalidation::None;
}

bool TryRoundCaptureRoi(tracing::core::Rect2 const& roi, tracing::capture::CanvasRoiRequest& request)
{
    if (!std::isfinite(roi.x) || !std::isfinite(roi.y) || !std::isfinite(roi.width) ||
        !std::isfinite(roi.height))
    {
        return false;
    }

    long long const x = std::llround(roi.x);
    long long const y = std::llround(roi.y);
    long long const w = std::llround(roi.width);
    long long const h = std::llround(roi.height);
    if (w <= 0 || h <= 0 || x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX ||
        w > INT_MAX || h > INT_MAX)
    {
        return false;
    }

    request.captureX = static_cast<int>(x);
    request.captureY = static_cast<int>(y);
    request.captureW = static_cast<int>(w);
    request.captureH = static_cast<int>(h);
    return true;
}

void SyncCanvasRoiRequest(ControlState& state)
{
    tracing::capture::CanvasRoiRequest request{};
    tracing::core::CaptureMappingInput const mapping = CoreMappingFromState(state);
    if (CalibrationIsLive(state) && tracing::core::CaptureMappingIsValidated(mapping))
    {
        std::optional<tracing::core::Rect2> const captureRoi =
            tracing::core::TryMapClientRoiToCapture(mapping, state.calibration.roi);
        if (captureRoi.has_value() && TryRoundCaptureRoi(*captureRoi, request))
        {
            request.applied = true;
            request.mappingValidated = true;
        }
    }
    state.capture.SetCanvasRoiRequest(request);
}

void ResetRoiFeed(ControlState& state) noexcept
{
    state.lastFedRoiSequence = 0;
    state.lastRoiFeedValid = false;
    state.lastRoiFeedAction = tracing::tracking::TrackingFrameAction::RejectNotCalibrated;
}

void ResetLiveCalibration(ControlState& state)
{
    state.calibration = LiveCalibration{};
    state.calibration.mRd = tracing::core::ResetReferenceAlignment();
    state.calibration.zoom = 1.0;
    state.tracking.Detach();
    ResetRoiFeed(state);
    SyncCanvasRoiRequest(state);
}

bool ReadEditDouble(HWND edit, double& value, bool allowEmpty, double emptyValue)
{
    value = emptyValue;
    if (edit == nullptr)
    {
        return allowEmpty;
    }
    wchar_t buffer[64]{};
    GetWindowTextW(edit, buffer, 64);
    if (buffer[0] == L'\0')
    {
        return allowEmpty;
    }
    wchar_t* end = nullptr;
    double const parsed = wcstod(buffer, &end);
    if (end == buffer || !std::isfinite(parsed))
    {
        return false;
    }
    value = parsed;
    return true;
}

tracing::core::Vec2 ClientOriginFromGeometry(tracing::platform::GeometrySnapshot const& geometry)
{
    return tracing::core::Vec2{
        static_cast<double>(geometry.clientPhysical.x),
        static_cast<double>(geometry.clientPhysical.y)};
}

bool TrackingUsesSnapshotMds(tracing::tracking::TrackingState state) noexcept
{
    return state != tracing::tracking::TrackingState::Unattached &&
           state != tracing::tracking::TrackingState::Unavailable;
}

void ApplyDerivedImagePlacement(ControlState& state)
{
    if (!state.renderer.HasTexture() || !CalibrationIsLive(state))
    {
        return;
    }

    tracing::core::Vec2 const clientOrigin = ClientOriginFromGeometry(state.lastGeometry);
    tracing::core::Vec2 const overlayOrigin =
        tracing::core::RoiScreenOrigin(clientOrigin, state.calibration.roi);
    tracing::core::Transform2D const mRd =
        tracing::core::MakeAlignedReferenceToDocument(state.calibration.mRd);
    tracing::tracking::TransformSnapshot const tracking = state.tracking.Snapshot();
    tracing::core::Transform2D const mDs = TrackingUsesSnapshotMds(tracking.state)
        ? tracking.mDs
        : tracing::core::MakeDocumentToScreen(
              overlayOrigin,
              state.calibration.canvasRadians,
              state.calibration.zoom,
              state.calibration.canvasFlipX,
              state.calibration.canvasFlipY,
              state.calibration.documentAnchor);
    tracing::core::Transform2D const mSo = tracing::core::MakeScreenToOverlay(overlayOrigin);
    std::optional<tracing::core::Transform2D> const mRs = tracing::core::Compose(mRd, mDs);
    std::optional<tracing::core::Transform2D> const mRo =
        mRs.has_value() ? tracing::core::Compose(*mRs, mSo) : std::nullopt;
    if (!mRo.has_value() || mRo->from != tracing::core::Space::R ||
        mRo->to != tracing::core::Space::O || !tracing::core::IsFinite(*mRo) ||
        tracing::core::IsSingular(*mRo))
    {
        state.calibration.axisAlignedPlacement = false;
        return;
    }

    tracing::graphics::ImagePlacement overlayPlacement{};
    overlayPlacement.useAffine = true;
    overlayPlacement.m00 = mRo->matrix.m[0];
    overlayPlacement.m01 = mRo->matrix.m[3];
    overlayPlacement.m10 = mRo->matrix.m[1];
    overlayPlacement.m11 = mRo->matrix.m[4];
    overlayPlacement.tx = mRo->matrix.m[6];
    overlayPlacement.ty = mRo->matrix.m[7];
    overlayPlacement.offsetX = overlayPlacement.tx;
    overlayPlacement.offsetY = overlayPlacement.ty;
    overlayPlacement.scale = overlayPlacement.m00;
    state.renderer.SetPlacement(overlayPlacement);
    state.calibration.axisAlignedPlacement =
        tracing::core::TryAxisAlignedOverlayPlacement(
            *mRo,
            static_cast<double>(state.renderer.TextureWidth()),
            static_cast<double>(state.renderer.TextureHeight()))
            .has_value();
}

tracing::core::CalibrationReport MakeCalibrationReport(ControlState const& state)
{
    tracing::core::CalibrationReport report{};
    report.roiApplied = state.calibration.roiApplied;
    report.roi = state.calibration.roi;
    tracing::core::Vec2 const clientOrigin = ClientOriginFromGeometry(state.lastGeometry);
    tracing::core::Vec2 const overlayOrigin = CalibrationIsLive(state)
        ? tracing::core::RoiScreenOrigin(clientOrigin, state.calibration.roi)
        : clientOrigin;
    report.clipO = tracing::core::OverlayClipO(
        state.calibration.roi, clientOrigin, overlayOrigin);
    report.units = tracing::core::ClassifyDocumentScale(
        state.calibration.declaredDocW, state.calibration.declaredDocH);
    report.declaredDocW = state.calibration.declaredDocW;
    report.declaredDocH = state.calibration.declaredDocH;
    report.mRd = state.calibration.mRd;
    report.documentAnchor = state.calibration.documentAnchor;
    report.zoom = state.calibration.zoom;
    report.canvasRadians = state.calibration.canvasRadians;
    report.canvasFlipX = state.calibration.canvasFlipX;
    report.canvasFlipY = state.calibration.canvasFlipY;
    report.mappingValidated =
        tracing::core::CaptureMappingIsValidated(CoreMappingFromState(state));
    report.invalidation = state.calibration.lastInvalidation;
    report.axisAlignedPlacement = state.calibration.axisAlignedPlacement;
    return report;
}

void RefreshCalibrationValidity(ControlState& state)
{
    if (!state.calibration.roiApplied || !state.selected.has_value())
    {
        return;
    }

    tracing::core::CalibrationSnapshot snap{};
    snap.targetGeneration = state.calibration.targetGeneration;
    snap.clientWidth = state.calibration.clientWidth;
    snap.clientHeight = state.calibration.clientHeight;
    snap.roi = state.calibration.roi;
    snap.mappingRequired = false;
    tracing::core::CalibrationInvalidation const reason =
        tracing::core::ShouldInvalidateCalibration(
            snap,
            state.selected->sessionGeneration,
            static_cast<double>(state.lastGeometry.clientPhysical.width),
            static_cast<double>(state.lastGeometry.clientPhysical.height),
            true);
    if (reason != tracing::core::CalibrationInvalidation::None)
    {
        state.calibration.roiApplied = false;
        state.calibration.lastInvalidation = reason;
        state.tracking.Detach();
        ResetRoiFeed(state);
    }
}

void SyncObsDiagnosticCheckboxes(ControlState& state);

void SyncPreviewCheckbox(ControlState& state)
{
    if (state.previewCheck != nullptr)
    {
        SendMessageW(
            state.previewCheck,
            BM_SETCHECK,
            state.preview.IsEnabled() ? BST_CHECKED : BST_UNCHECKED,
            0);
    }
}

void PresentPreview(ControlState& state)
{
    SyncPreviewCheckbox(state);
    if (!state.preview.IsEnabled())
    {
        return;
    }

    tracing::capture::FramePacket const packet = state.capture.LastPacket();
    bool const itemClosed =
        state.capture.State() == tracing::capture::CaptureSessionState::ItemClosed;
    tracing::graphics::CapturePreviewLabel const label = tracing::graphics::ClassifyCapturePreviewLabel(
        state.capture.HasOwnedFrame(),
        packet.stale,
        packet.contentWidth,
        packet.contentHeight,
        itemClosed);
    std::wstring error;
    state.preview.Present(
        state.capture.BorrowOwnedTexture(),
        label,
        packet.sequence,
        packet.captureTicks,
        MappingFromState(state),
        error);
}

std::wstring StatusHeader(ControlState const& state)
{
    return ControlDpiLine(state.control) + state.device.FormatReport() + L"\r\n" +
           state.overlay.FormatReport() + L"\r\n" + state.capture.FormatReport() + L"\r\n" +
           state.preview.FormatReport() + L"\r\n" +
           tracing::graphics::FormatCaptureToClientMapping(MappingFromState(state)) + L"\r\n" +
           state.image.FormatReport() + L"\r\n" +
           state.renderer.FormatReport() + L"\r\n" +
           state.reference.FormatReport() + L"\r\n" +
           L"overlayPresentPath=dxgi-hwnd (not ulw, not dcomp; OBS verification is user-recorded)\r\n"
           L"overlayHiddenOnCaptureLoss=" +
           std::wstring(state.hideOverlayOnCaptureLoss ? L"yes" : L"no") +
           L" obsSkipOverlayImagePresent=" +
           std::wstring(state.obsSkipOverlayImagePresent ? L"yes" : L"no") +
           L" (temporary OBS-gate diagnostic)\r\n"
           L"Transform diag: canned pR=(10,5)->pO=(2040,102); numerical only (not tracking)\r\n" +
           WidenAscii(tracing::core::FormatCalibrationReport(MakeCalibrationReport(state))) +
           L"\r\n" + WidenAscii(state.tracking.FormatReport()) + L"\r\n"
           L"roiFeed seq=" +
           std::to_wstring(state.lastFedRoiSequence) + L" valid=" +
           std::wstring(state.lastRoiFeedValid ? L"yes" : L"no") + L" action=" +
           WidenAscii(tracing::tracking::FormatTrackingFrameAction(state.lastRoiFeedAction)) +
           L"\r\n";
}

void SetStatus(ControlState& state, std::wstring const& text)
{
    if (state.status != nullptr)
    {
        SetWindowTextW(state.status, text.c_str());
    }
}

void FeedTrackingFromRoi(ControlState& state)
{
    tracing::tracking::TrackingState const trackingState = state.tracking.Snapshot().state;
    if (trackingState == tracing::tracking::TrackingState::Unattached ||
        trackingState == tracing::tracking::TrackingState::Unavailable ||
        trackingState == tracing::tracking::TrackingState::Paused)
    {
        return;
    }
    if (!state.capture.HasRoiBuffer())
    {
        return;
    }

    tracing::capture::RoiCpuSnapshot snapshot = state.capture.LastRoiBuffer();
    if (!snapshot.valid || snapshot.sequence == 0)
    {
        return;
    }
    if (state.lastRoiFeedValid && snapshot.sequence == state.lastFedRoiSequence)
    {
        return;
    }

    tracing::tracking::TrackingRoiFrame frame{};
    frame.meta.sequence = snapshot.sequence;
    frame.meta.captureTicks = snapshot.captureTicks;
    frame.meta.targetGeneration = snapshot.targetGeneration;
    frame.meta.geometryGeneration = snapshot.geometryGeneration;
    frame.meta.calibrationGeneration = state.calibration.calibrationGeneration;
    frame.meta.width = snapshot.width;
    frame.meta.height = snapshot.height;
    frame.meta.stride = snapshot.stride;
    frame.meta.downsample = snapshot.downsample < 1 ? 1 : snapshot.downsample;
    frame.bgra = std::move(snapshot.bgra);

    state.lastFedRoiSequence = snapshot.sequence;
    state.lastRoiFeedValid = true;
    state.lastRoiFeedAction = state.tracking.SubmitRoiFrame(std::move(frame));
}

void StopCapture(ControlState& state)
{
    state.capture.Stop();
    state.tracking.Detach();
    ResetRoiFeed(state);
    SyncCanvasRoiRequest(state);
    if (state.preview.IsEnabled())
    {
        std::wstring error;
        tracing::graphics::CaptureToClientMapping const mapping = MappingFromState(state);
        state.preview.Present(
            nullptr,
            tracing::graphics::CapturePreviewLabel::NoFrame,
            0,
            0,
            mapping,
            error);
    }
    else
    {
        state.preview.Hide();
    }
}

void StopWatching(ControlState& state)
{
    state.watcher.Detach();
    if (state.control != nullptr)
    {
        KillTimer(state.control, kTimerGeometry);
    }
    state.lastGeometry = {};
}

void SyncOverlayFromState(ControlState& state)
{
    tracing::graphics::OverlayPlacement placement{};
    bool targetUsable = false;
    bool targetForeground = false;
    bool const hasTarget = state.selected.has_value();
    HWND const foreground = GetForegroundWindow();
    bool const controlForeground = tracing::app::OverlayOwnerIsForeground(
        foreground,
        state.control,
        state.reference.Handle());

    if (hasTarget)
    {
        tracing::platform::GeometrySnapshot const& geometry = state.lastGeometry;
        tracing::platform::OverlayEligibility const eligibility =
            tracing::platform::ClassifyOverlayEligibility(geometry);
        targetUsable = eligibility == tracing::platform::OverlayEligibility::Eligible ||
                       eligibility == tracing::platform::OverlayEligibility::NotForeground;
        targetForeground = geometry.targetForeground;
        bool const imageReady =
            state.renderer.HasTexture() && !state.obsSkipOverlayImagePresent;
        if (CalibrationIsLive(state))
        {
            tracing::core::Vec2 const clientOrigin = ClientOriginFromGeometry(geometry);
            tracing::core::Vec2 const overlayOrigin =
                tracing::core::RoiScreenOrigin(clientOrigin, state.calibration.roi);
            placement.x = static_cast<long>(std::llround(overlayOrigin.x));
            placement.y = static_cast<long>(std::llround(overlayOrigin.y));
            placement.width = static_cast<long>(std::llround(state.calibration.roi.width));
            placement.height = static_cast<long>(std::llround(state.calibration.roi.height));
            if (imageReady)
            {
                ApplyDerivedImagePlacement(state);
            }
        }
        else if (!imageReady)
        {
            placement.x = geometry.clientPhysical.x;
            placement.y = geometry.clientPhysical.y;
            placement.width = geometry.clientPhysical.width;
            placement.height = geometry.clientPhysical.height;
        }
        else
        {
            placement = {};
            targetUsable = false;
        }
        if (state.hideOverlayOnCaptureLoss || state.tracking.Snapshot().hideOverlay)
        {
            targetUsable = false;
        }
    }
    else if (state.overlay.TestPatternActive())
    {
        placement = state.overlay.LastPlacement();
    }

    std::wstring error;
    state.overlay.UpdatePlacementAndVisibility(
        placement,
        targetUsable,
        targetForeground,
        controlForeground,
        hasTarget,
        error);
    if (state.overlay.IsVisible() && state.renderer.HasTexture() &&
        !state.obsSkipOverlayImagePresent)
    {
        std::wstring presentError;
        if (!state.renderer.DrawAndPresent(
                state.overlay,
                state.overlay.LastPlacement(),
                state.overlay.InteractionMode(),
                presentError) &&
            !presentError.empty())
        {
            OutputDebugStringW(presentError.c_str());
            OutputDebugStringW(L"\r\n");
        }
    }
    if (state.reference.IsVisible() && state.renderer.HasTexture())
    {
        std::wstring referenceError;
        if (!state.reference.Present(state.renderer, referenceError) && !referenceError.empty())
        {
            OutputDebugStringW(referenceError.c_str());
            OutputDebugStringW(L"\r\n");
        }
    }
}

void SetStatusWithOverlay(ControlState& state, std::wstring const& body)
{
    SyncPreviewCheckbox(state);
    SyncObsDiagnosticCheckboxes(state);
    SyncOverlayFromState(state);
    if (body.empty())
    {
        SetStatus(state, StatusHeader(state));
        return;
    }
    SetStatus(state, StatusHeader(state) + body);
}

void RefreshGeometryDisplay(ControlState& state, std::wstring const& extra)
{
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, extra);
        return;
    }

    tracing::platform::GeometrySnapshot sampled{};
    std::wstring error;
    bool const ok = tracing::platform::QueryPhysicalGeometry(*state.selected, sampled, error);
    if (!ok)
    {
        if (sampled.identity == tracing::platform::IdentityCheck::WindowDead ||
            sampled.identity == tracing::platform::IdentityCheck::HandleReused)
        {
            StopCapture(state);
            StopWatching(state);
            state.selected.reset();
            ResetLiveCalibration(state);
            std::wstring body = error;
            if (!extra.empty())
            {
                body += L"\r\n";
                body += extra;
            }
            SetStatusWithOverlay(state, body);
            return;
        }
        std::wstring body = error + L"\r\n" + tracing::platform::FormatGeometryReport(sampled);
        if (!extra.empty())
        {
            body += L"\r\n";
            body += extra;
        }
        SetStatusWithOverlay(state, body);
        return;
    }

    sampled.geometryGeneration =
        tracing::platform::NextGeometryGeneration(state.lastGeometry, sampled);
    state.lastGeometry = sampled;
    RefreshCalibrationValidity(state);
    state.capture.NoteGeometryGeneration(sampled.geometryGeneration);
    SyncCanvasRoiRequest(state);
    if (CalibrationIsLive(state))
    {
        tracing::core::Vec2 const overlayOrigin = tracing::core::RoiScreenOrigin(
            ClientOriginFromGeometry(sampled),
            state.calibration.roi);
        state.tracking.NoteGeometryGeneration(sampled.geometryGeneration);
        state.tracking.SetViewportAnchorS(overlayOrigin);
    }
    state.tracking.Tick(std::chrono::steady_clock::now());
    std::wstring body = tracing::platform::FormatGeometryReport(sampled);
    if (!extra.empty())
    {
        body += L"\r\n";
        body += extra;
    }
    SetStatusWithOverlay(state, body);
}

bool StartWatching(ControlState& state, std::wstring& hookError)
{
    hookError.clear();
    if (!state.selected.has_value() || state.control == nullptr)
    {
        hookError = L"Cannot watch geometry without a selected target.";
        return false;
    }

    StopWatching(state);
    bool const hooked = state.watcher.Attach(
        state.control,
        state.selected->hwnd,
        kMsgGeometry,
        hookError);
    SetTimer(state.control, kTimerGeometry, kGeometryPollMs, nullptr);
    return hooked;
}

void RefillList(ControlState& state)
{
    SendMessageW(state.list, LB_RESETCONTENT, 0, 0);
    for (std::size_t i = 0; i < state.candidates.size(); ++i)
    {
        tracing::platform::WindowCandidate const& candidate = state.candidates[i];
        if (candidate.kind == tracing::platform::CandidateKind::RejectedNotVisible)
        {
            continue;
        }
        std::wstring const line = tracing::platform::FormatCandidateLine(candidate);
        LRESULT const index = SendMessageW(
            state.list,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(line.c_str()));
        if (index != LB_ERR)
        {
            SendMessageW(state.list, LB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(i));
        }
    }
}

void RefreshCandidates(ControlState& state)
{
    std::wstring error;
    if (!tracing::platform::EnumerateTopLevelCandidates(state.candidates, error))
    {
        SetStatusWithOverlay(state, error);
        return;
    }
    RefillList(state);

    if (state.selected.has_value())
    {
        tracing::platform::TargetIdentity const& stored = *state.selected;
        bool const hwndAlive = IsWindow(stored.hwnd) != FALSE;
        tracing::platform::ProcessIdentity observed{};
        std::wstring accessError;
        bool observedKnown = false;
        if (hwndAlive)
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(stored.hwnd, &pid);
            observed.pid = pid;
            for (tracing::platform::WindowCandidate const& candidate : state.candidates)
            {
                if (candidate.hwnd == stored.hwnd)
                {
                    observed = candidate.process;
                    observedKnown = candidate.kind != tracing::platform::CandidateKind::AccessFailed &&
                                    candidate.process.creationTime != 0;
                    accessError = candidate.accessError;
                    break;
                }
            }
        }

        tracing::platform::IdentityCheck const check = tracing::platform::CheckIdentity(
            stored.process.pid,
            stored.process.creationTime,
            hwndAlive,
            observedKnown,
            observed.pid,
            observed.creationTime);
        if (check == tracing::platform::IdentityCheck::WindowDead)
        {
            StopCapture(state);
            StopWatching(state);
            state.selected.reset();
            ResetLiveCalibration(state);
            SetStatusWithOverlay(
                state,
                L"Previous target is gone. HWND is no longer valid; geometry invalidated. "
                L"Select a painting window again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::HandleReused)
        {
            StopCapture(state);
            StopWatching(state);
            state.selected.reset();
            ResetLiveCalibration(state);
            SetStatusWithOverlay(
                state,
                L"Previous HWND was reused by another process. Target and geometry cleared so "
                L"a launcher or second instance cannot silently replace it. Select again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::AccessFailed)
        {
            SetStatusWithOverlay(
                state,
                accessError.empty()
                    ? tracing::platform::FormatAccessFailure(
                          stored.process.pid, ERROR_ACCESS_DENIED)
                    : accessError);
            return;
        }

        RefreshGeometryDisplay(state, L"refreshed candidate list");
        return;
    }

    SetStatusWithOverlay(
        state,
        L"Refreshed top-level windows. Select a PAINT row (CLIPStudioPaint.exe), then Select. "
        L"Launcher rows are never chosen automatically.");
}

void SelectFromUi(ControlState& state)
{
    std::optional<std::size_t> userIndex;
    LRESULT const sel = SendMessageW(state.list, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR)
    {
        LRESULT const data = SendMessageW(state.list, LB_GETITEMDATA, static_cast<WPARAM>(sel), 0);
        if (data != LB_ERR)
        {
            userIndex = static_cast<std::size_t>(data);
        }
    }

    tracing::platform::TargetSelection const selection = tracing::platform::ResolveSelection(
        state.candidates,
        userIndex,
        state.nextGeneration);
    if (selection.status != tracing::platform::SelectionStatus::Selected)
    {
        SetStatusWithOverlay(state, selection.message);
        return;
    }

    StopCapture(state);
    StopWatching(state);
    state.selected = selection.identity;
    ++state.nextGeneration;
    ResetLiveCalibration(state);

    std::wstring hookError;
    bool const hooked = StartWatching(state, hookError);
    std::wstring extra = selection.message;
    if (!hooked)
    {
        extra += L"\r\n";
        extra += hookError;
    }
    RefreshGeometryDisplay(state, extra);
}

void OnEmergencyHide(ControlState& state)
{
    state.overlay.EmergencyHide();
    SetStatusWithOverlay(
        state,
        L"Emergency hide latched. Overlay stays hidden until Show test marker.");
}

void OnShowTestMarker(ControlState& state)
{
    state.hideOverlayOnCaptureLoss = false;
    state.overlay.ClearEmergencyHide();
    if (!state.selected.has_value())
    {
        state.overlay.RequestTestPattern();
        SetStatusWithOverlay(
            state,
            L"Show test marker: test-pattern surface (image renderer overwrites if uploaded).");
        return;
    }
    state.overlay.ClearTestPattern();
    RefreshGeometryDisplay(
        state,
        L"Show test marker: using selected target client physical bounds.");
}

void OnTracingMode(ControlState& state)
{
    state.overlay.SetInteractionMode(tracing::graphics::OverlayInteractionMode::Tracing);
    SetStatusWithOverlay(
        state,
        L"Interaction mode: tracing (noninteractive pass-through).");
}

void OnAlignmentMode(ControlState& state)
{
    state.overlay.SetInteractionMode(tracing::graphics::OverlayInteractionMode::Alignment);
    SetStatusWithOverlay(
        state,
        L"Interaction mode: alignment (overlay accepts input).");
}

void OnStartCapture(ControlState& state)
{
    if (state.control != nullptr)
    {
        PostMessageW(state.control, kMsgStartCapture, 0, 0);
    }
}

void OnStopCapture(ControlState& state)
{
    if (state.control != nullptr)
    {
        PostMessageW(state.control, kMsgStopCapture, 0, 0);
    }
}

void OnEnablePreview(ControlState& state)
{
    bool const checked =
        state.previewCheck != nullptr &&
        SendMessageW(state.previewCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.preview.SetEnabled(checked);
    if (checked)
    {
        PresentPreview(state);
        SetStatusWithOverlay(
            state,
            L"Capture preview enabled (ordinary WDA_NONE debug window; not for recording).");
        return;
    }
    SetStatusWithOverlay(state, L"Capture preview disabled (recording-safe default).");
}

void OnImportImage(ControlState& state)
{
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || !dialog)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: CoCreateInstance IFileOpenDialog failed hr=") + buffer);
        return;
    }

    COMDLG_FILTERSPEC const filters[] = {
        {L"PNG and JPEG", L"*.png;*.jpg;*.jpeg"},
        {L"PNG", L"*.png"},
        {L"JPEG", L"*.jpg;*.jpeg"},
    };
    dialog->SetFileTypes(ARRAYSIZE(filters), filters);
    dialog->SetTitle(L"Import reference PNG or JPEG");
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options)))
    {
        dialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    }

    hr = dialog->Show(state.control);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        SetStatusWithOverlay(state, L"Import image cancelled; previous image preserved.");
        return;
    }
    if (FAILED(hr))
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: IFileOpenDialog::Show failed hr=") + buffer);
        return;
    }

    Microsoft::WRL::ComPtr<IShellItem> item;
    hr = dialog->GetResult(&item);
    if (FAILED(hr) || !item)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: GetResult failed hr=") + buffer);
        return;
    }

    PWSTR filePath = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
    if (FAILED(hr) || filePath == nullptr)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: GetDisplayName failed hr=") + buffer);
        return;
    }
    std::wstring const path(filePath);
    CoTaskMemFree(filePath);

    std::wstring error;
    if (state.image.TryLoad(path, error))
    {
        std::wstring uploadError;
        if (state.image.Image() == nullptr || !state.renderer.Upload(*state.image.Image(), uploadError))
        {
            SetStatusWithOverlay(
                state,
                L"Imported CPU image; GPU upload failed; previous GPU texture preserved.\r\n" +
                    uploadError);
            return;
        }

        if (CalibrationIsLive(state))
        {
            bool const declared = state.calibration.declaredDocW > 0.0 &&
                                  state.calibration.declaredDocH > 0.0;
            state.calibration.mRd = tracing::core::FitReferenceInFrame(
                static_cast<double>(state.renderer.TextureWidth()),
                static_cast<double>(state.renderer.TextureHeight()),
                declared ? state.calibration.declaredDocW : state.calibration.roi.width,
                declared ? state.calibration.declaredDocH : state.calibration.roi.height);
            ApplyDerivedImagePlacement(state);
        }
        else
        {
            state.renderer.SetPlacement(tracing::graphics::ResetPlacement());
        }
        std::wstring referenceNote;
        if (state.renderer.HasTexture())
        {
            std::wstring showError;
            if (!state.reference.Show(state.renderer, showError))
            {
                referenceNote = L"\r\nReference window not shown: " + showError;
            }
            else
            {
                referenceNote =
                    L"\r\nOrdinary reference shown (WDA_NONE; independent fit; capture preview still off).";
            }
        }
        SetStatusWithOverlay(
            state,
            L"Imported image uploaded once (immutable texture; fit/reset/opacity do not re-upload)." +
                referenceNote);
        return;
    }
    SetStatusWithOverlay(
        state,
        L"Import failed; previous image preserved.\r\n" + error);
}

void SyncOpacityFromTrack(ControlState& state)
{
    if (state.opacityTrack == nullptr)
    {
        return;
    }
    int const pos = static_cast<int>(SendMessageW(state.opacityTrack, TBM_GETPOS, 0, 0));
    state.renderer.SetOpacity(static_cast<float>(pos) / 100.0f);
}

void OnOpacityChanged(ControlState& state)
{
    SyncOpacityFromTrack(state);
    SetStatusWithOverlay(
        state,
        L"Opacity updated (constant buffer only; texture generation unchanged).");
}

void OnFitImage(ControlState& state)
{
    if (!state.renderer.HasTexture())
    {
        SetStatusWithOverlay(state, L"Fit: no uploaded image.");
        return;
    }
    unsigned const generation = state.renderer.TextureGeneration();
    if (!CalibrationIsLive(state))
    {
        SetStatusWithOverlay(
            state,
            L"Fit: apply a canvas ROI first (client bounds are not canvas). textureGeneration=" +
                std::to_wstring(generation) + L" (unchanged).");
        return;
    }
    bool const declared =
        state.calibration.declaredDocW > 0.0 && state.calibration.declaredDocH > 0.0;
    state.calibration.mRd = tracing::core::FitReferenceInFrame(
        static_cast<double>(state.renderer.TextureWidth()),
        static_cast<double>(state.renderer.TextureHeight()),
        declared ? state.calibration.declaredDocW : state.calibration.roi.width,
        declared ? state.calibration.declaredDocH : state.calibration.roi.height);
    ApplyDerivedImagePlacement(state);
    SetStatusWithOverlay(
        state,
        L"Fit: uniform contain in ROI/declared document frame (M_RD). textureGeneration=" +
            std::to_wstring(generation) + L" (unchanged).");
}

void OnResetPlacement(ControlState& state)
{
    if (!state.renderer.HasTexture())
    {
        SetStatusWithOverlay(state, L"Reset: no uploaded image.");
        return;
    }
    unsigned const generation = state.renderer.TextureGeneration();
    state.calibration.mRd = tracing::core::ResetReferenceAlignment();
    if (CalibrationIsLive(state))
    {
        ApplyDerivedImagePlacement(state);
    }
    else
    {
        state.renderer.SetPlacement(tracing::graphics::ResetPlacement());
    }
    SetStatusWithOverlay(
        state,
        L"Reset: M_RD identity (offset 0, scale 1, no rot/flip). textureGeneration=" +
            std::to_wstring(generation) + L" (unchanged).");
}

void SyncObsDiagnosticCheckboxes(ControlState& state)
{
    if (state.obsPositiveControlCheck != nullptr)
    {
        bool const none =
            state.overlay.AffinityMode() ==
            tracing::graphics::OverlayAffinityMode::TemporaryNonePositiveControl;
        SendMessageW(
            state.obsPositiveControlCheck,
            BM_SETCHECK,
            none ? BST_CHECKED : BST_UNCHECKED,
            0);
    }
    if (state.obsSkipOverlayImageCheck != nullptr)
    {
        SendMessageW(
            state.obsSkipOverlayImageCheck,
            BM_SETCHECK,
            state.obsSkipOverlayImagePresent ? BST_CHECKED : BST_UNCHECKED,
            0);
    }
}

void OnObsPositiveControl(ControlState& state)
{
    bool const checked =
        state.obsPositiveControlCheck != nullptr &&
        SendMessageW(state.obsPositiveControlCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    auto const mode = checked
                          ? tracing::graphics::OverlayAffinityMode::TemporaryNonePositiveControl
                          : tracing::graphics::OverlayAffinityMode::ExcludeFromCapture;
    std::wstring error;
    bool const applied = state.overlay.SetAffinityMode(mode, error);
    SyncObsDiagnosticCheckboxes(state);
    if (!applied)
    {
        SetStatusWithOverlay(
            state,
            L"OBS positive-control affinity failed; overlay stays hidden.\r\n" + error);
        return;
    }
    if (checked)
    {
        SetStatusWithOverlay(
            state,
            L"Temporary OBS positive-control: overlay affinity WDA_NONE, presentPath=dxgi-hwnd. "
            L"Restore exclude before the exclusion recording. API success is not OBS proof.");
        return;
    }
    SetStatusWithOverlay(
        state,
        L"Restored WDA_EXCLUDEFROMCAPTURE before show (readback must be 0x11).");
}

void OnObsSkipOverlayImage(ControlState& state)
{
    state.obsSkipOverlayImagePresent =
        state.obsSkipOverlayImageCheck != nullptr &&
        SendMessageW(state.obsSkipOverlayImageCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    SetStatusWithOverlay(
        state,
        state.obsSkipOverlayImagePresent
            ? L"OBS overlay test-pattern: skip image present so magenta marker stays visible "
              L"(temporary diagnostic)."
            : L"OBS overlay test-pattern off: imported image presents onto the overlay.");
}

void OnRecreateOverlay(ControlState& state)
{
    if (!state.device.IsReady())
    {
        SetStatusWithOverlay(state, L"Recreate overlay HWND: D3D11 device is not ready.");
        return;
    }

    HWND const previous = state.overlay.Handle();
    std::wstring error;
    if (!state.overlay.Create(state.control, state.device, error))
    {
        SetStatusWithOverlay(state, L"Recreate overlay HWND failed.\r\n" + error);
        return;
    }
    SyncObsDiagnosticCheckboxes(state);
    wchar_t previousBuffer[32]{};
    wchar_t nextBuffer[32]{};
    swprintf_s(previousBuffer, L"0x%p", static_cast<void*>(previous));
    swprintf_s(nextBuffer, L"0x%p", static_cast<void*>(state.overlay.Handle()));
    SetStatusWithOverlay(
        state,
        std::wstring(L"Recreated overlay HWND ") + previousBuffer + L" -> " + nextBuffer +
            L" (affinity reapplied before show; mode=" +
            tracing::graphics::FormatOverlayAffinityMode(state.overlay.AffinityMode()) + L").");
}

void OnTransformDiag(ControlState& state)
{
    std::string const ascii = tracing::core::FormatCannedTransformDiagnostic();
    SetStatus(state, std::wstring(ascii.begin(), ascii.end()));
}

void OnApplyRoi(ControlState& state)
{
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, L"Apply ROI: select a PAINT HWND first.");
        return;
    }
    if (state.lastGeometry.clientPhysical.width <= 0 ||
        state.lastGeometry.clientPhysical.height <= 0)
    {
        SetStatusWithOverlay(state, L"Apply ROI: target client geometry is not valid yet.");
        return;
    }

    tracing::core::Rect2 roi{};
    if (!ReadEditDouble(state.roiXEdit, roi.x, false, 0.0) ||
        !ReadEditDouble(state.roiYEdit, roi.y, false, 0.0) ||
        !ReadEditDouble(state.roiWEdit, roi.width, false, 0.0) ||
        !ReadEditDouble(state.roiHEdit, roi.height, false, 0.0))
    {
        SetStatusWithOverlay(state, L"Apply ROI: enter numeric client-relative x y w h.");
        return;
    }

    double declaredW = 0.0;
    double declaredH = 0.0;
    if (!ReadEditDouble(state.docWEdit, declaredW, true, 0.0) ||
        !ReadEditDouble(state.docHEdit, declaredH, true, 0.0))
    {
        SetStatusWithOverlay(state, L"Apply ROI: document W/H must be empty or numeric.");
        return;
    }

    tracing::core::RoiRejectReason reason = tracing::core::RoiRejectReason::Ok;
    std::optional<tracing::core::Rect2> const valid = tracing::core::TryValidateCanvasRoi(
        roi,
        static_cast<double>(state.lastGeometry.clientPhysical.width),
        static_cast<double>(state.lastGeometry.clientPhysical.height),
        reason);
    if (!valid.has_value())
    {
        SetStatusWithOverlay(
            state,
            std::wstring(L"Apply ROI rejected: ") +
                WidenAscii(tracing::core::FormatRoiRejectReason(reason)) +
                L" (ROI is client-relative; client bounds are not canvas).");
        return;
    }

    state.calibration.roi = *valid;
    state.calibration.roiApplied = true;
    state.calibration.targetGeneration = state.selected->sessionGeneration;
    state.calibration.clientWidth =
        static_cast<double>(state.lastGeometry.clientPhysical.width);
    state.calibration.clientHeight =
        static_cast<double>(state.lastGeometry.clientPhysical.height);
    state.calibration.declaredDocW = declaredW;
    state.calibration.declaredDocH = declaredH;
    state.calibration.lastInvalidation = tracing::core::CalibrationInvalidation::None;
    ++state.calibration.calibrationGeneration;
    ApplyDerivedImagePlacement(state);
    SyncCanvasRoiRequest(state);
    SetStatusWithOverlay(
        state,
        L"Applied canvas ROI in client-relative pixels. Overlay HWND clipped to ROI. "
        L"Window move updates M_SO only. Start tracking after alignment.");
}

bool HandleCalibrationCommand(ControlState& state, int id)
{
    auto clampZoom = [](double zoom)
    {
        if (zoom < kMinZoom)
        {
            return kMinZoom;
        }
        if (zoom > kMaxZoom)
        {
            return kMaxZoom;
        }
        return zoom;
    };

    switch (id)
    {
    case kIdApplyRoi:
        OnApplyRoi(state);
        return true;
    case kIdAlignN:
        state.calibration.mRd.offsetD.y -= kAlignNudgePx;
        break;
    case kIdAlignS:
        state.calibration.mRd.offsetD.y += kAlignNudgePx;
        break;
    case kIdAlignW:
        state.calibration.mRd.offsetD.x -= kAlignNudgePx;
        break;
    case kIdAlignE:
        state.calibration.mRd.offsetD.x += kAlignNudgePx;
        break;
    case kIdAlignScaleDown:
        state.calibration.mRd.scale = clampZoom(state.calibration.mRd.scale / kScaleStep);
        break;
    case kIdAlignScaleUp:
        state.calibration.mRd.scale = clampZoom(state.calibration.mRd.scale * kScaleStep);
        break;
    case kIdAlignRotLeft:
        state.calibration.mRd.radiansClockwise -= kRotateStepRadians;
        break;
    case kIdAlignRotRight:
        state.calibration.mRd.radiansClockwise += kRotateStepRadians;
        break;
    case kIdAlignFlipX:
        state.calibration.mRd.flipX = !state.calibration.mRd.flipX;
        break;
    case kIdAlignFlipY:
        state.calibration.mRd.flipY = !state.calibration.mRd.flipY;
        break;
    case kIdCanvasN:
        state.calibration.documentAnchor.y -= kCanvasNudgePx;
        break;
    case kIdCanvasS:
        state.calibration.documentAnchor.y += kCanvasNudgePx;
        break;
    case kIdCanvasW:
        state.calibration.documentAnchor.x -= kCanvasNudgePx;
        break;
    case kIdCanvasE:
        state.calibration.documentAnchor.x += kCanvasNudgePx;
        break;
    case kIdCanvasZoomDown:
        state.calibration.zoom = clampZoom(state.calibration.zoom / kScaleStep);
        break;
    case kIdCanvasZoomUp:
        state.calibration.zoom = clampZoom(state.calibration.zoom * kScaleStep);
        break;
    case kIdCanvasRotLeft:
        state.calibration.canvasRadians -= kRotateStepRadians;
        break;
    case kIdCanvasRotRight:
        state.calibration.canvasRadians += kRotateStepRadians;
        break;
    case kIdCanvasFlipX:
        state.calibration.canvasFlipX = !state.calibration.canvasFlipX;
        break;
    case kIdCanvasFlipY:
        state.calibration.canvasFlipY = !state.calibration.canvasFlipY;
        break;
    default:
        return false;
    }

    ApplyDerivedImagePlacement(state);
    SetStatusWithOverlay(
        state,
        L"Manual calibration updated (M_RD alignment vs M_DS canvas). Tracking uses the "
        L"current M_DS only after Start tracking.");
    return true;
}

void OnStartTracking(ControlState& state)
{
    if (!CalibrationIsLive(state) || !state.selected.has_value())
    {
        SetStatusWithOverlay(
            state,
            L"Start tracking: apply a live canvas ROI on a selected target first.");
        return;
    }

    tracing::core::Vec2 const overlayOrigin = tracing::core::RoiScreenOrigin(
        ClientOriginFromGeometry(state.lastGeometry),
        state.calibration.roi);
    tracing::core::Transform2D const mDs = tracing::core::MakeDocumentToScreen(
        overlayOrigin,
        state.calibration.canvasRadians,
        state.calibration.zoom,
        state.calibration.canvasFlipX,
        state.calibration.canvasFlipY,
        state.calibration.documentAnchor);
    tracing::core::Transform2D const mSo = tracing::core::MakeScreenToOverlay(overlayOrigin);
    std::string error;
    ResetRoiFeed(state);
    if (!state.tracking.BeginCalibrated(
            state.selected->sessionGeneration,
            state.lastGeometry.geometryGeneration,
            state.calibration.calibrationGeneration,
            mDs,
            mSo,
            overlayOrigin,
            error))
    {
        SetStatusWithOverlay(state, L"Start tracking failed: " + WidenAscii(error));
        return;
    }
    FeedTrackingFromRoi(state);
    SetStatusWithOverlay(
        state,
        L"Tracking Calibrating. ROI CPU buffers feed the worker; hide-on-Lost is armed.");
}

void OnPauseTracking(ControlState& state)
{
    tracing::tracking::TrackingState const current = state.tracking.Snapshot().state;
    if (current == tracing::tracking::TrackingState::Paused)
    {
        state.tracking.Resume();
        SetStatusWithOverlay(state, L"Tracking resumed.");
        return;
    }
    state.tracking.Pause();
    SetStatusWithOverlay(state, L"Tracking paused.");
}

void OnResyncTracking(ControlState& state)
{
    state.tracking.Resync();
    if (CalibrationIsLive(state) && state.selected.has_value())
    {
        OnStartTracking(state);
        return;
    }
    SetStatusWithOverlay(state, L"Tracking resync: detached (apply ROI, then Start tracking).");
}

void OnShowReference(ControlState& state)
{
    if (!state.renderer.HasTexture())
    {
        SetStatusWithOverlay(state, L"Show reference: no uploaded image.");
        return;
    }
    std::wstring error;
    if (!state.reference.Show(state.renderer, error))
    {
        SetStatusWithOverlay(state, L"Show reference failed.\r\n" + error);
        return;
    }
    SetStatusWithOverlay(
        state,
        L"Ordinary reference shown (WDA_NONE; independent of overlay fit/opacity/tracking; capture preview remains off).");
}

void StartCaptureNow(ControlState& state)
{
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, L"Select a PAINT HWND before Start Capture.");
        return;
    }
    if (!state.device.IsReady())
    {
        SetStatusWithOverlay(state, L"Start Capture requires a ready D3D11 device.");
        return;
    }
    if (state.lastGeometry.identity != tracing::platform::IdentityCheck::Match)
    {
        RefreshGeometryDisplay(state, L"");
    }
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, L"Target became invalid before Start Capture.");
        return;
    }

    std::wstring error;
    if (!state.capture.Start(
            state.selected->hwnd,
            state.selected->sessionGeneration,
            state.lastGeometry.geometryGeneration,
            state.device,
            state.control,
            kMsgCapture,
            error))
    {
        SetStatusWithOverlay(state, error);
        return;
    }
    state.hideOverlayOnCaptureLoss = false;
    SyncCanvasRoiRequest(state);
    RefreshGeometryDisplay(
        state,
        L"WGC capture started (owned frames; preview off by default). Overlay test-pattern "
        L"is not a captured frame.");
}

std::wstring FormatMonitorDevice(POINT origin)
{
    HMONITOR const monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) == FALSE)
    {
        return L"monitor=unknown";
    }
    return std::wstring(L"monitor=") + info.szDevice;
}

std::wstring FormatVirtualDesktop()
{
    return L"virtualScreen origin=(" +
           std::to_wstring(GetSystemMetrics(SM_XVIRTUALSCREEN)) + L"," +
           std::to_wstring(GetSystemMetrics(SM_YVIRTUALSCREEN)) + L") size=" +
           std::to_wstring(GetSystemMetrics(SM_CXVIRTUALSCREEN)) + L"x" +
           std::to_wstring(GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

void OnCoverInputProbe(ControlState& state)
{
    HWND const probe = FindWindowW(L"TracingAppInputProbe", nullptr);
    if (probe == nullptr)
    {
        SetStatusWithOverlay(
            state,
            L"Input probe not running. Start TracingApp.InputProbe.exe first.");
        return;
    }

    RECT client{};
    if (GetClientRect(probe, &client) == FALSE || client.right <= client.left ||
        client.bottom <= client.top)
    {
        SetStatusWithOverlay(state, L"Input probe client rect is empty.");
        return;
    }

    POINT origin{client.left, client.top};
    if (ClientToScreen(probe, &origin) == FALSE)
    {
        SetStatusWithOverlay(state, L"ClientToScreen failed for input probe.");
        return;
    }

    StopCapture(state);
    StopWatching(state);
    state.selected.reset();
    ResetLiveCalibration(state);
    state.overlay.ClearEmergencyHide();

    tracing::graphics::OverlayPlacement placement{};
    placement.x = origin.x;
    placement.y = origin.y;
    placement.width = client.right - client.left;
    placement.height = client.bottom - client.top;
    state.overlay.RequestTestPatternAt(placement);

    DWORD probePid = 0;
    GetWindowThreadProcessId(probe, &probePid);
    SetStatusWithOverlay(
        state,
        L"Cover input probe: test-pattern over TracingAppInputProbe PID " +
            std::to_wstring(probePid) +
            L" (different process; not a CSP target).\r\nprobe client origin=(" +
            std::to_wstring(origin.x) + L"," + std::to_wstring(origin.y) + L") size=" +
            std::to_wstring(placement.width) + L"x" + std::to_wstring(placement.height) +
            L"\r\n" + FormatMonitorDevice(origin) + L" " + FormatVirtualDesktop() +
            L"\r\nSendInput must use virtual-desktop absolute mapping, not primary-only.");
}

LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<ControlState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        auto* created = new ControlState();
        created->control = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
        created->list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            12,
            12,
            680,
            200,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdList)),
            instance,
            nullptr);
        created->status = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            492,
            680,
            420,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatus)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Refresh",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            220,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRefresh)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Select",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            124,
            220,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSelect)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Emergency Hide",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            236,
            220,
            140,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEmergencyHide)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Show test marker",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            384,
            220,
            150,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShowMarker)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Tracing mode",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            252,
            130,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTracingMode)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Alignment mode",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            150,
            252,
            140,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAlignmentMode)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Cover input probe",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            298,
            252,
            160,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCoverProbe)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Start Capture",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            466,
            252,
            104,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStartCapture)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Stop Capture",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            576,
            252,
            104,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStopCapture)),
            instance,
            nullptr);
        created->previewCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"Enable capture preview",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            12,
            284,
            220,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEnablePreview)),
            instance,
            nullptr);
        SendMessageW(created->previewCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Import image",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            240,
            284,
            140,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdImportImage)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"STATIC",
            L"Opacity",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            316,
            60,
            24,
            hwnd,
            nullptr,
            instance,
            nullptr);
        created->opacityTrack = CreateWindowExW(
            0,
            TRACKBAR_CLASSW,
            L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
            76,
            312,
            280,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOpacity)),
            instance,
            nullptr);
        SendMessageW(created->opacityTrack, TBM_SETRANGEMIN, FALSE, 0);
        SendMessageW(created->opacityTrack, TBM_SETRANGEMAX, FALSE, 100);
        SendMessageW(created->opacityTrack, TBM_SETPOS, TRUE, 100);
        SendMessageW(created->opacityTrack, TBM_SETTICFREQ, 25, 0);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Fit",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            364,
            312,
            80,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdFit)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Reset",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            452,
            312,
            80,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdResetPlacement)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Show reference",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            540,
            312,
            152,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShowReference)),
            instance,
            nullptr);
        created->obsPositiveControlCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"OBS +ve WDA_NONE (temp)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            12,
            348,
            210,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdObsPositiveControl)),
            instance,
            nullptr);
        SendMessageW(created->obsPositiveControlCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        created->obsSkipOverlayImageCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"Skip overlay image (OBS)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            230,
            348,
            220,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdObsSkipOverlayImage)),
            instance,
            nullptr);
        SendMessageW(created->obsSkipOverlayImageCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Recreate overlay HWND",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            458,
            344,
            234,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRecreateOverlay)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Transform diag",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            376,
            160,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTransformDiag)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Start tracking",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            180,
            376,
            124,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStartTracking)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Pause",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            310,
            376,
            72,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPauseTracking)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Resync",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            388,
            376,
            72,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdResyncTracking)),
            instance,
            nullptr);
        auto addButton = [&](wchar_t const* title, int x, int y, int w, int h, int id)
        {
            CreateWindowExW(
                0,
                L"BUTTON",
                title,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                x,
                y,
                w,
                h,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance,
                nullptr);
        };
        auto addEdit = [&](int x, int y, int w, int id, wchar_t const* initial) -> HWND
        {
            return CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                initial,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                x,
                y,
                w,
                22,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance,
                nullptr);
        };
        CreateWindowExW(
            0,
            L"STATIC",
            L"ROI",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            406,
            28,
            20,
            hwnd,
            nullptr,
            instance,
            nullptr);
        created->roiXEdit = addEdit(42, 404, 58, kIdRoiX, L"80");
        created->roiYEdit = addEdit(102, 404, 58, kIdRoiY, L"80");
        created->roiWEdit = addEdit(162, 404, 58, kIdRoiW, L"640");
        created->roiHEdit = addEdit(222, 404, 58, kIdRoiH, L"480");
        addButton(L"Apply ROI", 286, 402, 90, 24, kIdApplyRoi);
        CreateWindowExW(
            0,
            L"STATIC",
            L"Doc px",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            384,
            406,
            48,
            20,
            hwnd,
            nullptr,
            instance,
            nullptr);
        created->docWEdit = addEdit(434, 404, 70, kIdDocW, L"");
        created->docHEdit = addEdit(506, 404, 70, kIdDocH, L"");
        CreateWindowExW(
            0,
            L"STATIC",
            L"M_RD",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            436,
            40,
            20,
            hwnd,
            nullptr,
            instance,
            nullptr);
        addButton(L"N", 54, 432, 28, 24, kIdAlignN);
        addButton(L"S", 84, 432, 28, 24, kIdAlignS);
        addButton(L"W", 114, 432, 28, 24, kIdAlignW);
        addButton(L"E", 144, 432, 28, 24, kIdAlignE);
        addButton(L"-", 178, 432, 28, 24, kIdAlignScaleDown);
        addButton(L"+", 208, 432, 28, 24, kIdAlignScaleUp);
        addButton(L"R-", 242, 432, 32, 24, kIdAlignRotLeft);
        addButton(L"R+", 276, 432, 32, 24, kIdAlignRotRight);
        addButton(L"FX", 314, 432, 32, 24, kIdAlignFlipX);
        addButton(L"FY", 348, 432, 32, 24, kIdAlignFlipY);
        CreateWindowExW(
            0,
            L"STATIC",
            L"M_DS",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            464,
            40,
            20,
            hwnd,
            nullptr,
            instance,
            nullptr);
        addButton(L"N", 54, 460, 28, 24, kIdCanvasN);
        addButton(L"S", 84, 460, 28, 24, kIdCanvasS);
        addButton(L"W", 114, 460, 28, 24, kIdCanvasW);
        addButton(L"E", 144, 460, 28, 24, kIdCanvasE);
        addButton(L"-", 178, 460, 28, 24, kIdCanvasZoomDown);
        addButton(L"+", 208, 460, 28, 24, kIdCanvasZoomUp);
        addButton(L"R-", 242, 460, 32, 24, kIdCanvasRotLeft);
        addButton(L"R+", 276, 460, 32, 24, kIdCanvasRotRight);
        addButton(L"FX", 314, 460, 32, 24, kIdCanvasFlipX);
        addButton(L"FY", 348, 460, 32, 24, kIdCanvasFlipY);
        CreateWindowExW(
            0,
            L"STATIC",
            L"blank Doc=local units; M_DS zoom is relative not CSP %; rot/flip via overlay-local affine + RS scissor",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            388,
            434,
            300,
            52,
            hwnd,
            nullptr,
            instance,
            nullptr);
        std::wstring deviceError;
        if (!created->device.Create(deviceError))
        {
            OutputDebugStringW(deviceError.c_str());
            OutputDebugStringW(L"\r\n");
        }
        else
        {
            std::wstring overlayError;
            if (!created->overlay.Create(hwnd, created->device, overlayError))
            {
                OutputDebugStringW(overlayError.c_str());
                OutputDebugStringW(L"\r\n");
            }
            else
            {
                std::wstring previewError;
                if (!created->preview.Create(hwnd, created->device, previewError))
                {
                    OutputDebugStringW(previewError.c_str());
                    OutputDebugStringW(L"\r\n");
                }
                std::wstring rendererError;
                if (!created->renderer.Create(created->device, rendererError))
                {
                    OutputDebugStringW(rendererError.c_str());
                    OutputDebugStringW(L"\r\n");
                }
                else
                {
                    std::wstring referenceError;
                    if (!created->reference.Create(hwnd, created->device, referenceError))
                    {
                        OutputDebugStringW(referenceError.c_str());
                        OutputDebugStringW(L"\r\n");
                    }
                }
            }
        }
        RefreshCandidates(*created);
        return 0;
    }
    case WM_COMMAND:
        if (state != nullptr)
        {
            int const id = LOWORD(wParam);
            int const code = HIWORD(wParam);
            if (id == kIdRefresh && code == BN_CLICKED)
            {
                RefreshCandidates(*state);
                return 0;
            }
            if (id == kIdSelect && code == BN_CLICKED)
            {
                SelectFromUi(*state);
                return 0;
            }
            if (id == kIdEmergencyHide && code == BN_CLICKED)
            {
                OnEmergencyHide(*state);
                return 0;
            }
            if (id == kIdShowMarker && code == BN_CLICKED)
            {
                OnShowTestMarker(*state);
                return 0;
            }
            if (id == kIdTracingMode && code == BN_CLICKED)
            {
                OnTracingMode(*state);
                return 0;
            }
            if (id == kIdAlignmentMode && code == BN_CLICKED)
            {
                OnAlignmentMode(*state);
                return 0;
            }
            if (id == kIdCoverProbe && code == BN_CLICKED)
            {
                OnCoverInputProbe(*state);
                return 0;
            }
            if (id == kIdStartCapture && code == BN_CLICKED)
            {
                OnStartCapture(*state);
                return 0;
            }
            if (id == kIdStopCapture && code == BN_CLICKED)
            {
                OnStopCapture(*state);
                return 0;
            }
            if (id == kIdEnablePreview && code == BN_CLICKED)
            {
                OnEnablePreview(*state);
                return 0;
            }
            if (id == kIdImportImage && code == BN_CLICKED)
            {
                OnImportImage(*state);
                return 0;
            }
            if (id == kIdFit && code == BN_CLICKED)
            {
                OnFitImage(*state);
                return 0;
            }
            if (id == kIdResetPlacement && code == BN_CLICKED)
            {
                OnResetPlacement(*state);
                return 0;
            }
            if (id == kIdShowReference && code == BN_CLICKED)
            {
                OnShowReference(*state);
                return 0;
            }
            if (id == kIdObsPositiveControl && code == BN_CLICKED)
            {
                OnObsPositiveControl(*state);
                return 0;
            }
            if (id == kIdObsSkipOverlayImage && code == BN_CLICKED)
            {
                OnObsSkipOverlayImage(*state);
                return 0;
            }
            if (id == kIdRecreateOverlay && code == BN_CLICKED)
            {
                OnRecreateOverlay(*state);
                return 0;
            }
            if (id == kIdTransformDiag && code == BN_CLICKED)
            {
                OnTransformDiag(*state);
                return 0;
            }
            if (id == kIdStartTracking && code == BN_CLICKED)
            {
                OnStartTracking(*state);
                return 0;
            }
            if (id == kIdPauseTracking && code == BN_CLICKED)
            {
                OnPauseTracking(*state);
                return 0;
            }
            if (id == kIdResyncTracking && code == BN_CLICKED)
            {
                OnResyncTracking(*state);
                return 0;
            }
            if (code == BN_CLICKED && HandleCalibrationCommand(*state, id))
            {
                return 0;
            }
            if (id == kIdList && code == LBN_DBLCLK)
            {
                SelectFromUi(*state);
                return 0;
            }
        }
        break;
    case WM_HSCROLL:
        if (state != nullptr && state->opacityTrack != nullptr &&
            reinterpret_cast<HWND>(lParam) == state->opacityTrack)
        {
            OnOpacityChanged(*state);
            return 0;
        }
        break;
    case kMsgGeometry:
        if (state != nullptr && state->selected.has_value())
        {
            RefreshGeometryDisplay(*state, L"WinEvent");
            return 0;
        }
        break;
    case kMsgCapture:
        if (state != nullptr)
        {
            if (wParam == 1)
            {
                state->capture.OnItemClosed();
                state->tracking.MarkUnavailable();
                ResetRoiFeed(*state);
                state->hideOverlayOnCaptureLoss = true;
                PresentPreview(*state);
                RefreshGeometryDisplay(*state, L"WGC item Closed; session torn down; tracing overlay hidden.");
                return 0;
            }
            RefreshCalibrationValidity(*state);
            SyncCanvasRoiRequest(*state);
            state->capture.PumpHandoff();
            FeedTrackingFromRoi(*state);
            tracing::capture::FramePacket const packet = state->capture.LastPacket();
            if (packet.stale)
            {
                state->hideOverlayOnCaptureLoss = true;
            }
            PresentPreview(*state);
            RefreshGeometryDisplay(*state, L"");
            return 0;
        }
        break;
    case kMsgStartCapture:
        if (state != nullptr)
        {
            StartCaptureNow(*state);
            return 0;
        }
        break;
    case kMsgStopCapture:
        if (state != nullptr)
        {
            StopCapture(*state);
            SetStatusWithOverlay(*state, L"WGC capture stopped.");
            return 0;
        }
        break;
    case WM_TIMER:
        if (state != nullptr && wParam == kTimerGeometry && state->selected.has_value())
        {
            RefreshGeometryDisplay(*state, L"");
            return 0;
        }
        break;
    case WM_DPICHANGED:
        if (state != nullptr)
        {
            RefreshGeometryDisplay(*state, L"control DPI changed");
        }
        break;
    case WM_DESTROY:
        if (state != nullptr)
        {
            StopCapture(*state);
            state->tracking.Stop();
            StopWatching(*state);
            state->reference.Release();
            state->preview.Release();
            state->renderer.Release();
            state->overlay.Release();
            state->device.Release();
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    if (!IsValidControlViewport(kDefaultWidth, kDefaultHeight))
    {
        return 1;
    }

    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&common);

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ControlWndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    HWND const window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kDefaultWidth,
        kDefaultHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

#endif
