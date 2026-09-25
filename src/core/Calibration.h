#pragma once

#include "core/Transform2D.h"

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

} // namespace tracing::core
