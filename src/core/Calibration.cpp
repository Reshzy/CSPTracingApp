#include "core/Calibration.h"

#include <cmath>
#include <cstdio>

namespace tracing::core {
namespace {

constexpr double kAxisAlignedEpsilon = 1e-9;
constexpr double kMinPositiveScale = 1e-12;

bool IsFiniteNumber(double value) noexcept
{
    return std::isfinite(value);
}

bool IsFiniteVec(Vec2 value) noexcept
{
    return IsFiniteNumber(value.x) && IsFiniteNumber(value.y);
}

bool IsFiniteRect(Rect2 const& rect) noexcept
{
    return IsFiniteNumber(rect.x) && IsFiniteNumber(rect.y) && IsFiniteNumber(rect.width) &&
           IsFiniteNumber(rect.height);
}

void Append(std::string& text, char const* format, double value)
{
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), format, value);
    text += buffer;
}

Transform2D ComposeOrIdentity(
    Transform2D const& first,
    Transform2D const& then,
    Space from,
    Space to)
{
    std::optional<Transform2D> const composed = Compose(first, then);
    return composed.value_or(Identity(from, to));
}

} // namespace

char const* FormatRoiRejectReason(RoiRejectReason reason) noexcept
{
    switch (reason)
    {
    case RoiRejectReason::Ok:
        return "ok";
    case RoiRejectReason::NonFinite:
        return "non-finite";
    case RoiRejectReason::NonPositiveSize:
        return "non-positive-size";
    case RoiRejectReason::Overflow:
        return "overflow";
    case RoiRejectReason::OutOfClient:
        return "out-of-client";
    }
    return "unknown";
}

char const* FormatCaptureMappingMatch(CaptureMappingMatch match) noexcept
{
    switch (match)
    {
    case CaptureMappingMatch::None:
        return "none";
    case CaptureMappingMatch::OuterWindow:
        return "outer";
    case CaptureMappingMatch::DwmFrame:
        return "dwm";
    case CaptureMappingMatch::OuterAndDwm:
        return "outer-and-dwm";
    }
    return "unknown";
}

char const* FormatDocumentScaleLabel(DocumentScaleLabel label) noexcept
{
    switch (label)
    {
    case DocumentScaleLabel::DeclaredDocumentPixels:
        return "declared document pixels (not verified CSP px; zoom is relative, not CSP %)";
    case DocumentScaleLabel::LocalCalibratedUnits:
        return "local calibrated units (not verified document pixels or CSP zoom %)";
    }
    return "unknown-units";
}

char const* FormatCalibrationInvalidation(CalibrationInvalidation reason) noexcept
{
    switch (reason)
    {
    case CalibrationInvalidation::None:
        return "none";
    case CalibrationInvalidation::TargetGenerationChanged:
        return "target-generation-changed";
    case CalibrationInvalidation::ClientSizeChanged:
        return "client-size-changed";
    case CalibrationInvalidation::RoiOutOfBounds:
        return "roi-out-of-bounds";
    case CalibrationInvalidation::MappingUnvalidated:
        return "mapping-unvalidated";
    }
    return "unknown";
}

DocumentScaleLabel ClassifyDocumentScale(double declaredWidth, double declaredHeight) noexcept
{
    if (IsFiniteNumber(declaredWidth) && IsFiniteNumber(declaredHeight) && declaredWidth > 0.0 &&
        declaredHeight > 0.0)
    {
        return DocumentScaleLabel::DeclaredDocumentPixels;
    }
    return DocumentScaleLabel::LocalCalibratedUnits;
}

std::optional<Rect2> TryValidateCanvasRoi(
    Rect2 roi,
    double clientWidth,
    double clientHeight,
    RoiRejectReason& reason)
{
    reason = RoiRejectReason::Ok;
    if (!IsFiniteRect(roi) || !IsFiniteNumber(clientWidth) || !IsFiniteNumber(clientHeight))
    {
        reason = RoiRejectReason::NonFinite;
        return std::nullopt;
    }
    if (clientWidth <= 0.0 || clientHeight <= 0.0 || roi.width <= 0.0 || roi.height <= 0.0)
    {
        reason = RoiRejectReason::NonPositiveSize;
        return std::nullopt;
    }

    double const right = roi.x + roi.width;
    double const bottom = roi.y + roi.height;
    if (!IsFiniteNumber(right) || !IsFiniteNumber(bottom))
    {
        reason = RoiRejectReason::Overflow;
        return std::nullopt;
    }
    if (roi.x < 0.0 || roi.y < 0.0 || right > clientWidth || bottom > clientHeight)
    {
        reason = RoiRejectReason::OutOfClient;
        return std::nullopt;
    }
    return roi;
}

bool CaptureMappingIsValidated(CaptureMappingInput const& input) noexcept
{
    if (input.match == CaptureMappingMatch::None)
    {
        return false;
    }
    if (!IsFiniteNumber(input.contentWidth) || !IsFiniteNumber(input.contentHeight) ||
        !IsFiniteNumber(input.clientWidth) || !IsFiniteNumber(input.clientHeight) ||
        !IsFiniteVec(input.clientOriginS) || !IsFiniteVec(input.captureToClient))
    {
        return false;
    }
    return input.contentWidth > 0.0 && input.contentHeight > 0.0 && input.clientWidth > 0.0 &&
           input.clientHeight > 0.0;
}

std::optional<Transform2D> TryMakeCaptureToScreen(CaptureMappingInput const& input)
{
    if (!CaptureMappingIsValidated(input))
    {
        return std::nullopt;
    }
    return Translate(
        Space::C,
        Space::S,
        input.clientOriginS.x - input.captureToClient.x,
        input.clientOriginS.y - input.captureToClient.y);
}

std::optional<Vec2> TryMapCaptureToClient(CaptureMappingInput const& input, Vec2 capturePoint)
{
    if (!CaptureMappingIsValidated(input) || !IsFiniteVec(capturePoint))
    {
        return std::nullopt;
    }
    return Vec2{
        capturePoint.x - input.captureToClient.x,
        capturePoint.y - input.captureToClient.y};
}

std::optional<Vec2> TryMapCaptureToScreen(CaptureMappingInput const& input, Vec2 capturePoint)
{
    std::optional<Transform2D> const transform = TryMakeCaptureToScreen(input);
    if (!transform.has_value())
    {
        return std::nullopt;
    }
    return Apply(*transform, capturePoint);
}

Transform2D MakeAlignedReferenceToDocument(ReferenceAlignment const& alignment)
{
    if (!IsFiniteVec(alignment.offsetD) || !IsFiniteNumber(alignment.scale) ||
        !IsFiniteNumber(alignment.radiansClockwise) || alignment.scale <= kMinPositiveScale)
    {
        return Identity(Space::R, Space::D);
    }

    Transform2D const flip = Flip(Space::R, Space::R, alignment.flipX, alignment.flipY);
    Transform2D const scale = UniformScale(Space::R, Space::R, alignment.scale);
    Transform2D const rotate =
        RotateClockwise(Space::R, Space::R, alignment.radiansClockwise);
    Transform2D const offset =
        Translate(Space::R, Space::D, alignment.offsetD.x, alignment.offsetD.y);

    Transform2D composed = ComposeOrIdentity(flip, scale, Space::R, Space::R);
    composed = ComposeOrIdentity(composed, rotate, Space::R, Space::R);
    return ComposeOrIdentity(composed, offset, Space::R, Space::D);
}

ReferenceAlignment FitReferenceInFrame(
    double imageWidth,
    double imageHeight,
    double frameWidth,
    double frameHeight)
{
    ReferenceAlignment alignment = ResetReferenceAlignment();
    if (!IsFiniteNumber(imageWidth) || !IsFiniteNumber(imageHeight) ||
        !IsFiniteNumber(frameWidth) || !IsFiniteNumber(frameHeight) || imageWidth <= 0.0 ||
        imageHeight <= 0.0 || frameWidth <= 0.0 || frameHeight <= 0.0)
    {
        return alignment;
    }

    double const scaleX = frameWidth / imageWidth;
    double const scaleY = frameHeight / imageHeight;
    alignment.scale = scaleX < scaleY ? scaleX : scaleY;
    alignment.offsetD.x = (frameWidth - imageWidth * alignment.scale) * 0.5;
    alignment.offsetD.y = (frameHeight - imageHeight * alignment.scale) * 0.5;
    return alignment;
}

ReferenceAlignment ResetReferenceAlignment() noexcept
{
    return ReferenceAlignment{};
}

Vec2 RoiScreenOrigin(Vec2 clientOriginS, Rect2 roi) noexcept
{
    return Vec2{clientOriginS.x + roi.x, clientOriginS.y + roi.y};
}

Rect2 OverlayClipO(Rect2 roiClient, Vec2 clientOriginS, Vec2 overlayOriginS) noexcept
{
    Rect2 clip{};
    clip.x = roiClient.x + clientOriginS.x - overlayOriginS.x;
    clip.y = roiClient.y + clientOriginS.y - overlayOriginS.y;
    clip.width = roiClient.width;
    clip.height = roiClient.height;
    return clip;
}

bool LandmarkInsideClip(Vec2 overlayPoint, Rect2 clipO, double epsilon) noexcept
{
    if (!IsFiniteVec(overlayPoint) || !IsFiniteRect(clipO) || !IsFiniteNumber(epsilon))
    {
        return false;
    }
    return overlayPoint.x + epsilon >= clipO.x && overlayPoint.y + epsilon >= clipO.y &&
           overlayPoint.x <= clipO.x + clipO.width + epsilon &&
           overlayPoint.y <= clipO.y + clipO.height + epsilon;
}

CalibrationInvalidation ShouldInvalidateCalibration(
    CalibrationSnapshot const& calibrated,
    std::uint64_t currentTargetGeneration,
    double currentClientWidth,
    double currentClientHeight,
    bool mappingValidatedWhenRequired) noexcept
{
    if (currentTargetGeneration != calibrated.targetGeneration)
    {
        return CalibrationInvalidation::TargetGenerationChanged;
    }
    if (!IsFiniteNumber(currentClientWidth) || !IsFiniteNumber(currentClientHeight) ||
        currentClientWidth != calibrated.clientWidth ||
        currentClientHeight != calibrated.clientHeight)
    {
        return CalibrationInvalidation::ClientSizeChanged;
    }

    RoiRejectReason reason = RoiRejectReason::Ok;
    if (!TryValidateCanvasRoi(
            calibrated.roi, currentClientWidth, currentClientHeight, reason)
             .has_value())
    {
        return CalibrationInvalidation::RoiOutOfBounds;
    }
    if (calibrated.mappingRequired && !mappingValidatedWhenRequired)
    {
        return CalibrationInvalidation::MappingUnvalidated;
    }
    return CalibrationInvalidation::None;
}

std::optional<AxisAlignedPlacement> TryAxisAlignedOverlayPlacement(
    Transform2D const& mRo,
    double imageWidth,
    double imageHeight)
{
    if (mRo.from != Space::R || mRo.to != Space::O)
    {
        return std::nullopt;
    }
    if (!IsFinite(mRo) || IsSingular(mRo) || !IsFiniteNumber(imageWidth) ||
        !IsFiniteNumber(imageHeight) || imageWidth <= 0.0 || imageHeight <= 0.0)
    {
        return std::nullopt;
    }

    double const a = mRo.matrix.m[0];
    double const b = mRo.matrix.m[1];
    double const c = mRo.matrix.m[3];
    double const d = mRo.matrix.m[4];
    if (std::fabs(b) > kAxisAlignedEpsilon || std::fabs(c) > kAxisAlignedEpsilon)
    {
        return std::nullopt;
    }
    if (a <= kMinPositiveScale || d <= kMinPositiveScale)
    {
        return std::nullopt;
    }
    if (std::fabs(a - d) > kAxisAlignedEpsilon)
    {
        return std::nullopt;
    }

    std::optional<Vec2> const origin = Apply(mRo, Vec2{0.0, 0.0});
    if (!origin.has_value())
    {
        return std::nullopt;
    }

    AxisAlignedPlacement placement{};
    placement.offsetX = origin->x;
    placement.offsetY = origin->y;
    placement.scale = a;
    return placement;
}

std::string FormatCalibrationReport(CalibrationReport const& report)
{
    std::string text;
    text += "tracking=disabled (manual calibration only; no visual estimator)\r\n";
    text += "units=";
    text += FormatDocumentScaleLabel(report.units);
    text += "\r\nroiApplied=";
    text += report.roiApplied ? "yes" : "no";
    text += " roi=(";
    Append(text, "%.3f", report.roi.x);
    text += ",";
    Append(text, "%.3f", report.roi.y);
    text += " ";
    Append(text, "%.3f", report.roi.width);
    text += "x";
    Append(text, "%.3f", report.roi.height);
    text += ") clipO=(";
    Append(text, "%.3f", report.clipO.x);
    text += ",";
    Append(text, "%.3f", report.clipO.y);
    text += " ";
    Append(text, "%.3f", report.clipO.width);
    text += "x";
    Append(text, "%.3f", report.clipO.height);
    text += ")\r\ndeclaredDoc=";
    Append(text, "%.3f", report.declaredDocW);
    text += "x";
    Append(text, "%.3f", report.declaredDocH);
    text += " mappingValidated=";
    text += report.mappingValidated ? "yes" : "no";
    text += " invalidation=";
    text += FormatCalibrationInvalidation(report.invalidation);
    text += "\r\nM_RD offset=(";
    Append(text, "%.3f", report.mRd.offsetD.x);
    text += ",";
    Append(text, "%.3f", report.mRd.offsetD.y);
    text += ") scale=";
    Append(text, "%.5f", report.mRd.scale);
    text += " rad=";
    Append(text, "%.5f", report.mRd.radiansClockwise);
    text += " flipX=";
    text += report.mRd.flipX ? "yes" : "no";
    text += " flipY=";
    text += report.mRd.flipY ? "yes" : "no";
    text += "\r\nM_DS documentAnchor=(";
    Append(text, "%.3f", report.documentAnchor.x);
    text += ",";
    Append(text, "%.3f", report.documentAnchor.y);
    text += ") zoom=";
    Append(text, "%.5f", report.zoom);
    text += " (relative, not CSP %) rad=";
    Append(text, "%.5f", report.canvasRadians);
    text += " flipX=";
    text += report.canvasFlipX ? "yes" : "no";
    text += " flipY=";
    text += report.canvasFlipY ? "yes" : "no";
    text += "\r\nwindow move updates M_SO/viewportAnchor not M_RD; axisAlignedPlacement=";
    text += report.axisAlignedPlacement ? "yes" : "no";
    if (!report.axisAlignedPlacement)
    {
        text += " (rotation/flip numerical until ImageRenderer affine substep)";
    }
    return text;
}

} // namespace tracing::core
