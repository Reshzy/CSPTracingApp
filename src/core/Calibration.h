#pragma once

#include "core/Transform2D.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace tracing::core {

// Canvas ROI is client-relative physical pixels. Not outer/DWM bounds and not
// an assumption that the client rectangle is the painting canvas.

struct Rect2
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

enum class RoiRejectReason
{
    Ok,
    NonFinite,
    NonPositiveSize,
    Overflow,
    OutOfClient,
};

enum class CaptureMappingMatch
{
    None,
    OuterWindow,
    DwmFrame,
    OuterAndDwm,
};

struct CaptureMappingInput
{
    double contentWidth = 0.0;
    double contentHeight = 0.0;
    Vec2 clientOriginS{};
    double clientWidth = 0.0;
    double clientHeight = 0.0;
    Vec2 captureToClient{};
    CaptureMappingMatch match = CaptureMappingMatch::None;
};

enum class DocumentScaleLabel
{
    LocalCalibratedUnits,
    DeclaredDocumentPixels,
};

enum class CalibrationInvalidation
{
    None,
    TargetGenerationChanged,
    ClientSizeChanged,
    RoiOutOfBounds,
    MappingUnvalidated,
};

struct CalibrationSnapshot
{
    std::uint64_t targetGeneration = 0;
    double clientWidth = 0.0;
    double clientHeight = 0.0;
    Rect2 roi{};
    bool mappingRequired = false;
};

struct ReferenceAlignment
{
    Vec2 offsetD{};
    double scale = 1.0;
    double radiansClockwise = 0.0;
    bool flipX = false;
    bool flipY = false;
};

struct AxisAlignedPlacement
{
    double offsetX = 0.0;
    double offsetY = 0.0;
    double scale = 1.0;
};

struct CalibrationReport
{
    bool roiApplied = false;
    Rect2 roi{};
    Rect2 clipO{};
    DocumentScaleLabel units = DocumentScaleLabel::LocalCalibratedUnits;
    double declaredDocW = 0.0;
    double declaredDocH = 0.0;
    ReferenceAlignment mRd{};
    Vec2 documentAnchor{};
    double zoom = 1.0;
    double canvasRadians = 0.0;
    bool canvasFlipX = false;
    bool canvasFlipY = false;
    bool mappingValidated = false;
    CalibrationInvalidation invalidation = CalibrationInvalidation::None;
    bool axisAlignedPlacement = true;
};

char const* FormatRoiRejectReason(RoiRejectReason reason) noexcept;
char const* FormatCaptureMappingMatch(CaptureMappingMatch match) noexcept;
char const* FormatDocumentScaleLabel(DocumentScaleLabel label) noexcept;
char const* FormatCalibrationInvalidation(CalibrationInvalidation reason) noexcept;

DocumentScaleLabel ClassifyDocumentScale(double declaredWidth, double declaredHeight) noexcept;

std::optional<Rect2> TryValidateCanvasRoi(
    Rect2 roi,
    double clientWidth,
    double clientHeight,
    RoiRejectReason& reason);

bool CaptureMappingIsValidated(CaptureMappingInput const& input) noexcept;
std::optional<Transform2D> TryMakeCaptureToScreen(CaptureMappingInput const& input);
std::optional<Vec2> TryMapCaptureToClient(CaptureMappingInput const& input, Vec2 capturePoint);
std::optional<Vec2> TryMapCaptureToScreen(CaptureMappingInput const& input, Vec2 capturePoint);
std::optional<Vec2> TryMapClientToCapture(CaptureMappingInput const& input, Vec2 clientPoint);
std::optional<Rect2> TryMapClientRoiToCapture(CaptureMappingInput const& input, Rect2 clientRoi);

Transform2D MakeAlignedReferenceToDocument(ReferenceAlignment const& alignment);
ReferenceAlignment FitReferenceInFrame(
    double imageWidth,
    double imageHeight,
    double frameWidth,
    double frameHeight);
ReferenceAlignment ResetReferenceAlignment() noexcept;

Vec2 RoiScreenOrigin(Vec2 clientOriginS, Rect2 roi) noexcept;
Rect2 OverlayClipO(Rect2 roiClient, Vec2 clientOriginS, Vec2 overlayOriginS) noexcept;
bool LandmarkInsideClip(Vec2 overlayPoint, Rect2 clipO, double epsilon = 1e-9) noexcept;

CalibrationInvalidation ShouldInvalidateCalibration(
    CalibrationSnapshot const& calibrated,
    std::uint64_t currentTargetGeneration,
    double currentClientWidth,
    double currentClientHeight,
    bool mappingValidatedWhenRequired) noexcept;

std::optional<AxisAlignedPlacement> TryAxisAlignedOverlayPlacement(
    Transform2D const& mRo,
    double imageWidth,
    double imageHeight);

std::string FormatCalibrationReport(CalibrationReport const& report);

inline std::optional<Vec2> TryMapClientToCapture(CaptureMappingInput const& input, Vec2 clientPoint)
{
    if (!CaptureMappingIsValidated(input) || !std::isfinite(clientPoint.x) ||
        !std::isfinite(clientPoint.y))
    {
        return std::nullopt;
    }

    Vec2 capture{clientPoint.x + input.captureToClient.x, clientPoint.y + input.captureToClient.y};
    if (!std::isfinite(capture.x) || !std::isfinite(capture.y))
    {
        return std::nullopt;
    }
    return capture;
}

inline std::optional<Rect2> TryMapClientRoiToCapture(
    CaptureMappingInput const& input,
    Rect2 clientRoi)
{
    if (!std::isfinite(clientRoi.x) || !std::isfinite(clientRoi.y) ||
        !std::isfinite(clientRoi.width) || !std::isfinite(clientRoi.height) ||
        clientRoi.width <= 0.0 || clientRoi.height <= 0.0)
    {
        return std::nullopt;
    }

    std::optional<Vec2> const origin =
        TryMapClientToCapture(input, Vec2{clientRoi.x, clientRoi.y});
    if (!origin.has_value())
    {
        return std::nullopt;
    }

    Rect2 captureRoi{};
    captureRoi.x = origin->x;
    captureRoi.y = origin->y;
    captureRoi.width = clientRoi.width;
    captureRoi.height = clientRoi.height;
    if (!std::isfinite(captureRoi.x + captureRoi.width) ||
        !std::isfinite(captureRoi.y + captureRoi.height))
    {
        return std::nullopt;
    }
    return captureRoi;
}

} // namespace tracing::core
