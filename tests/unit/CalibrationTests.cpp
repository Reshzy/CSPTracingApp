#include <gtest/gtest.h>

#include "core/Calibration.h"
#include "core/Transform2D.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace {

using tracing::core::Apply;
using tracing::core::AxisAlignedPlacement;
using tracing::core::CalibrationInvalidation;
using tracing::core::CalibrationReport;
using tracing::core::CalibrationSnapshot;
using tracing::core::CaptureMappingInput;
using tracing::core::CaptureMappingIsValidated;
using tracing::core::CaptureMappingMatch;
using tracing::core::ClassifyDocumentScale;
using tracing::core::Compose;
using tracing::core::DocumentScaleLabel;
using tracing::core::FitReferenceInFrame;
using tracing::core::FormatCalibrationInvalidation;
using tracing::core::FormatCalibrationReport;
using tracing::core::FormatDocumentScaleLabel;
using tracing::core::LandmarkInsideClip;
using tracing::core::MakeAlignedReferenceToDocument;
using tracing::core::MakeDocumentToScreen;
using tracing::core::MakeScreenToOverlay;
using tracing::core::MapReferenceToOverlay;
using tracing::core::OverlayClipO;
using tracing::core::Rect2;
using tracing::core::ReferenceAlignment;
using tracing::core::ResetReferenceAlignment;
using tracing::core::RoiRejectReason;
using tracing::core::RoiScreenOrigin;
using tracing::core::ShouldInvalidateCalibration;
using tracing::core::Space;
using tracing::core::Transform2D;
using tracing::core::TryAxisAlignedOverlayPlacement;
using tracing::core::TryMakeCaptureToScreen;
using tracing::core::TryMapCaptureToClient;
using tracing::core::TryMapCaptureToScreen;
using tracing::core::TryMapClientRoiToCapture;
using tracing::core::TryMapClientToCapture;
using tracing::core::TryValidateCanvasRoi;
using tracing::core::Vec2;

constexpr double kPi = std::numbers::pi_v<double>;

void ExpectVecNear(Vec2 actual, double x, double y, double absError = 1e-9)
{
    EXPECT_NEAR(actual.x, x, absError);
    EXPECT_NEAR(actual.y, y, absError);
}

Vec2 MustApply(Transform2D const& transform, Vec2 point)
{
    std::optional<Vec2> const mapped = Apply(transform, point);
    EXPECT_TRUE(mapped.has_value());
    return mapped.value_or(Vec2{});
}

Rect2 MustValidate(Rect2 roi, double clientWidth, double clientHeight)
{
    RoiRejectReason reason = RoiRejectReason::Ok;
    std::optional<Rect2> const valid = TryValidateCanvasRoi(roi, clientWidth, clientHeight, reason);
    EXPECT_TRUE(valid.has_value());
    EXPECT_EQ(reason, RoiRejectReason::Ok);
    return valid.value_or(Rect2{});
}

} // namespace

TEST(CalibrationRoi, AcceptsInteriorClientRelativeRectangle)
{
    Rect2 const roi = MustValidate(Rect2{40.0, 80.0, 320.0, 240.0}, 800.0, 600.0);
    EXPECT_DOUBLE_EQ(roi.x, 40.0);
    EXPECT_DOUBLE_EQ(roi.width, 320.0);
}

TEST(CalibrationRoi, RejectsZeroAndNegativeSize)
{
    RoiRejectReason reason = RoiRejectReason::Ok;
    EXPECT_FALSE(TryValidateCanvasRoi(Rect2{0.0, 0.0, 0.0, 10.0}, 100.0, 100.0, reason).has_value());
    EXPECT_EQ(reason, RoiRejectReason::NonPositiveSize);
    EXPECT_FALSE(TryValidateCanvasRoi(Rect2{0.0, 0.0, 10.0, -1.0}, 100.0, 100.0, reason).has_value());
    EXPECT_EQ(reason, RoiRejectReason::NonPositiveSize);
}

TEST(CalibrationRoi, RejectsEmptyClient)
{
    RoiRejectReason reason = RoiRejectReason::Ok;
    EXPECT_FALSE(TryValidateCanvasRoi(Rect2{0.0, 0.0, 10.0, 10.0}, 0.0, 100.0, reason).has_value());
    EXPECT_EQ(reason, RoiRejectReason::NonPositiveSize);
}

TEST(CalibrationRoi, RejectsPastClientEdge)
{
    RoiRejectReason reason = RoiRejectReason::Ok;
    EXPECT_FALSE(TryValidateCanvasRoi(Rect2{50.0, 0.0, 60.0, 10.0}, 100.0, 100.0, reason).has_value());
    EXPECT_EQ(reason, RoiRejectReason::OutOfClient);
    EXPECT_FALSE(TryValidateCanvasRoi(Rect2{-1.0, 0.0, 10.0, 10.0}, 100.0, 100.0, reason).has_value());
    EXPECT_EQ(reason, RoiRejectReason::OutOfClient);
}

TEST(CalibrationRoi, RejectsNonFiniteAndOverflow)
{
    RoiRejectReason reason = RoiRejectReason::Ok;
    double const inf = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(TryValidateCanvasRoi(Rect2{inf, 0.0, 10.0, 10.0}, 100.0, 100.0, reason).has_value());
    EXPECT_EQ(reason, RoiRejectReason::NonFinite);
    EXPECT_FALSE(
        TryValidateCanvasRoi(Rect2{1.0e308, 0.0, 1.0e308, 10.0}, 1.0e308, 100.0, reason).has_value());
    EXPECT_TRUE(reason == RoiRejectReason::Overflow || reason == RoiRejectReason::OutOfClient);
}

TEST(CalibrationMapping, CaptureToClientToScreenWithNegativeOrigin)
{
    CaptureMappingInput input{};
    input.contentWidth = 1920.0;
    input.contentHeight = 1080.0;
    input.clientOriginS = Vec2{-1920.0, 108.0};
    input.clientWidth = 1912.0;
    input.clientHeight = 1049.0;
    input.captureToClient = Vec2{8.0, 31.0};
    input.match = CaptureMappingMatch::OuterWindow;
    ASSERT_TRUE(CaptureMappingIsValidated(input));

    std::optional<Vec2> const client = TryMapCaptureToClient(input, Vec2{10.0, 20.0});
    ASSERT_TRUE(client.has_value());
    ExpectVecNear(*client, 2.0, -11.0);

    std::optional<Vec2> const screen = TryMapCaptureToScreen(input, Vec2{10.0, 20.0});
    ASSERT_TRUE(screen.has_value());
    ExpectVecNear(*screen, -1918.0, 97.0);

    std::optional<Transform2D> const mCs = TryMakeCaptureToScreen(input);
    ASSERT_TRUE(mCs.has_value());
    EXPECT_EQ(mCs->from, Space::C);
    EXPECT_EQ(mCs->to, Space::S);

    std::optional<Vec2> const back = TryMapClientToCapture(input, *client);
    ASSERT_TRUE(back.has_value());
    ExpectVecNear(*back, 10.0, 20.0);
}

TEST(CalibrationMapping, RejectsUnmatchedAndZeroContent)
{
    CaptureMappingInput input{};
    input.contentWidth = 1920.0;
    input.contentHeight = 1080.0;
    input.clientOriginS = Vec2{10.0, 20.0};
    input.clientWidth = 800.0;
    input.clientHeight = 600.0;
    input.captureToClient = Vec2{8.0, 31.0};
    input.match = CaptureMappingMatch::None;
    EXPECT_FALSE(CaptureMappingIsValidated(input));
    EXPECT_FALSE(TryMakeCaptureToScreen(input).has_value());

    input.match = CaptureMappingMatch::DwmFrame;
    input.contentWidth = 0.0;
    EXPECT_FALSE(CaptureMappingIsValidated(input));
    EXPECT_FALSE(TryMapCaptureToClient(input, Vec2{1.0, 1.0}).has_value());
    EXPECT_FALSE(TryMapClientToCapture(input, Vec2{1.0, 1.0}).has_value());
    EXPECT_FALSE(TryMapClientRoiToCapture(input, Rect2{80.0, 80.0, 640.0, 480.0}).has_value());
}

TEST(CalibrationMapping, ClientRoiToCaptureUsesMeasuredOffset)
{
    CaptureMappingInput input{};
    input.contentWidth = 1920.0;
    input.contentHeight = 1080.0;
    input.clientOriginS = Vec2{-1920.0, 108.0};
    input.clientWidth = 1912.0;
    input.clientHeight = 1049.0;
    input.captureToClient = Vec2{8.0, 31.0};
    input.match = CaptureMappingMatch::OuterWindow;

    std::optional<Rect2> const capture =
        TryMapClientRoiToCapture(input, Rect2{80.0, 80.0, 640.0, 480.0});
    ASSERT_TRUE(capture.has_value());
    EXPECT_DOUBLE_EQ(capture->x, 88.0);
    EXPECT_DOUBLE_EQ(capture->y, 111.0);
    EXPECT_DOUBLE_EQ(capture->width, 640.0);
    EXPECT_DOUBLE_EQ(capture->height, 480.0);

    std::optional<Vec2> const clientOrigin = TryMapCaptureToClient(input, Vec2{capture->x, capture->y});
    ASSERT_TRUE(clientOrigin.has_value());
    ExpectVecNear(*clientOrigin, 80.0, 80.0);
}

TEST(CalibrationMapping, ClientRoiRejectsUnvalidatedAndNonPositive)
{
    CaptureMappingInput input{};
    input.contentWidth = 1920.0;
    input.contentHeight = 1080.0;
    input.clientOriginS = Vec2{10.0, 20.0};
    input.clientWidth = 800.0;
    input.clientHeight = 600.0;
    input.captureToClient = Vec2{8.0, 31.0};
    input.match = CaptureMappingMatch::None;
    EXPECT_FALSE(TryMapClientRoiToCapture(input, Rect2{80.0, 80.0, 640.0, 480.0}).has_value());

    input.match = CaptureMappingMatch::OuterWindow;
    EXPECT_FALSE(TryMapClientRoiToCapture(input, Rect2{80.0, 80.0, 0.0, 480.0}).has_value());
    EXPECT_FALSE(TryMapClientRoiToCapture(input, Rect2{80.0, 80.0, 640.0, -1.0}).has_value());
}

TEST(CalibrationUnits, DistinguishesLocalFromDeclaredDocumentPixels)
{
    EXPECT_EQ(ClassifyDocumentScale(0.0, 0.0), DocumentScaleLabel::LocalCalibratedUnits);
    EXPECT_EQ(ClassifyDocumentScale(-10.0, 100.0), DocumentScaleLabel::LocalCalibratedUnits);
    EXPECT_EQ(ClassifyDocumentScale(4096.0, 2304.0), DocumentScaleLabel::DeclaredDocumentPixels);
    std::string const declared = FormatDocumentScaleLabel(DocumentScaleLabel::DeclaredDocumentPixels);
    EXPECT_NE(declared.find("not verified"), std::string::npos);
    EXPECT_EQ(declared.find("verified CSP zoom"), std::string::npos);
    std::string const local = FormatDocumentScaleLabel(DocumentScaleLabel::LocalCalibratedUnits);
    EXPECT_NE(local.find("not verified document pixels"), std::string::npos);
}

TEST(CalibrationInvalidate, WindowMoveDoesNotInvalidateMrd)
{
    CalibrationSnapshot snap{};
    snap.targetGeneration = 4;
    snap.clientWidth = 800.0;
    snap.clientHeight = 600.0;
    snap.roi = Rect2{50.0, 40.0, 400.0, 300.0};
    EXPECT_EQ(
        ShouldInvalidateCalibration(snap, 4, 800.0, 600.0, true),
        CalibrationInvalidation::None);

    ReferenceAlignment alignment = ResetReferenceAlignment();
    alignment.offsetD = Vec2{12.0, 34.0};
    alignment.scale = 1.5;
    Transform2D const mRdBefore = MakeAlignedReferenceToDocument(alignment);
    Transform2D const mRdAfterMove = MakeAlignedReferenceToDocument(alignment);
    Vec2 const before = MustApply(mRdBefore, Vec2{10.0, 5.0});
    Vec2 const after = MustApply(mRdAfterMove, Vec2{10.0, 5.0});
    ExpectVecNear(before, after.x, after.y);

    Vec2 const originA{10.0, 20.0};
    Vec2 const originB{1000.0, 20.0};
    Rect2 const clipA = OverlayClipO(snap.roi, originA, RoiScreenOrigin(originA, snap.roi));
    Rect2 const clipB = OverlayClipO(snap.roi, originB, RoiScreenOrigin(originB, snap.roi));
    EXPECT_DOUBLE_EQ(clipA.x, 0.0);
    EXPECT_DOUBLE_EQ(clipA.y, 0.0);
    EXPECT_DOUBLE_EQ(clipB.x, 0.0);
    EXPECT_DOUBLE_EQ(clipB.width, 400.0);
    EXPECT_NE(RoiScreenOrigin(originA, snap.roi).x, RoiScreenOrigin(originB, snap.roi).x);
}

TEST(CalibrationInvalidate, ClientSizeAndTargetGenerationInvalidate)
{
    CalibrationSnapshot snap{};
    snap.targetGeneration = 2;
    snap.clientWidth = 800.0;
    snap.clientHeight = 600.0;
    snap.roi = Rect2{0.0, 0.0, 100.0, 100.0};
    EXPECT_EQ(
        ShouldInvalidateCalibration(snap, 3, 800.0, 600.0, true),
        CalibrationInvalidation::TargetGenerationChanged);
    EXPECT_EQ(
        ShouldInvalidateCalibration(snap, 2, 1024.0, 600.0, true),
        CalibrationInvalidation::ClientSizeChanged);
    EXPECT_STREQ(
        FormatCalibrationInvalidation(CalibrationInvalidation::ClientSizeChanged),
        "client-size-changed");
}

TEST(CalibrationInvalidate, MappingRequiredRejectsUnvalidated)
{
    CalibrationSnapshot snap{};
    snap.targetGeneration = 1;
    snap.clientWidth = 100.0;
    snap.clientHeight = 100.0;
    snap.roi = Rect2{0.0, 0.0, 50.0, 50.0};
    snap.mappingRequired = true;
    EXPECT_EQ(
        ShouldInvalidateCalibration(snap, 1, 100.0, 100.0, false),
        CalibrationInvalidation::MappingUnvalidated);
    EXPECT_EQ(
        ShouldInvalidateCalibration(snap, 1, 100.0, 100.0, true),
        CalibrationInvalidation::None);
}

TEST(CalibrationChain, LandmarkStaysInsideClipAfterPanZoomIdentityParity)
{
    Rect2 const roi = MustValidate(Rect2{50.0, 40.0, 400.0, 300.0}, 800.0, 600.0);
    Vec2 const clientOrigin{10.0, 20.0};
    Vec2 const overlayOrigin = RoiScreenOrigin(clientOrigin, roi);
    Rect2 const clip = OverlayClipO(roi, clientOrigin, overlayOrigin);
    EXPECT_DOUBLE_EQ(clip.x, 0.0);
    EXPECT_DOUBLE_EQ(clip.width, 400.0);

    ReferenceAlignment alignment = ResetReferenceAlignment();
    Transform2D const mRd = MakeAlignedReferenceToDocument(alignment);
    Transform2D const mDs = MakeDocumentToScreen(
        overlayOrigin, 0.0, 1.0, false, false, Vec2{0.0, 0.0});
    Transform2D const mSo = MakeScreenToOverlay(overlayOrigin);
    std::optional<Vec2> const pO = MapReferenceToOverlay(mRd, mDs, mSo, Vec2{10.0, 5.0});
    ASSERT_TRUE(pO.has_value());
    ExpectVecNear(*pO, 10.0, 5.0);
    EXPECT_TRUE(LandmarkInsideClip(*pO, clip));

    Vec2 const movedOrigin{1920.0, 20.0};
    Vec2 const movedOverlay = RoiScreenOrigin(movedOrigin, roi);
    Transform2D const mDsMoved = MakeDocumentToScreen(
        movedOverlay, 0.0, 1.0, false, false, Vec2{0.0, 0.0});
    Transform2D const mSoMoved = MakeScreenToOverlay(movedOverlay);
    std::optional<Vec2> const pOMoved =
        MapReferenceToOverlay(mRd, mDsMoved, mSoMoved, Vec2{10.0, 5.0});
    ASSERT_TRUE(pOMoved.has_value());
    ExpectVecNear(*pOMoved, 10.0, 5.0);
    EXPECT_TRUE(LandmarkInsideClip(*pOMoved, OverlayClipO(roi, movedOrigin, movedOverlay)));
}

TEST(CalibrationPlacement, AxisAlignedUniformScaleAcceptedRotationRejected)
{
    ReferenceAlignment alignment = ResetReferenceAlignment();
    alignment.offsetD = Vec2{8.0, 16.0};
    alignment.scale = 2.0;
    Transform2D const mRd = MakeAlignedReferenceToDocument(alignment);
    Transform2D const mDs =
        MakeDocumentToScreen(Vec2{100.0, 200.0}, 0.0, 1.0, false, false, Vec2{0.0, 0.0});
    Transform2D const mSo = MakeScreenToOverlay(Vec2{100.0, 200.0});
    std::optional<Transform2D> const mRs = Compose(mRd, mDs);
    ASSERT_TRUE(mRs.has_value());
    std::optional<Transform2D> const mRo = Compose(*mRs, mSo);
    ASSERT_TRUE(mRo.has_value());

    std::optional<AxisAlignedPlacement> const placement =
        TryAxisAlignedOverlayPlacement(*mRo, 64.0, 32.0);
    ASSERT_TRUE(placement.has_value());
    EXPECT_NEAR(placement->scale, 2.0, 1e-9);
    EXPECT_NEAR(placement->offsetX, 8.0, 1e-9);
    EXPECT_NEAR(placement->offsetY, 16.0, 1e-9);

    alignment.radiansClockwise = kPi / 2.0;
    Transform2D const rotated = MakeAlignedReferenceToDocument(alignment);
    std::optional<Transform2D> const rotatedRs = Compose(rotated, mDs);
    ASSERT_TRUE(rotatedRs.has_value());
    std::optional<Transform2D> const rotatedRo = Compose(*rotatedRs, mSo);
    ASSERT_TRUE(rotatedRo.has_value());
    EXPECT_FALSE(TryAxisAlignedOverlayPlacement(*rotatedRo, 64.0, 32.0).has_value());
}

TEST(CalibrationFit, ContainCentersInFrame)
{
    ReferenceAlignment const fit = FitReferenceInFrame(100.0, 50.0, 200.0, 200.0);
    EXPECT_NEAR(fit.scale, 2.0, 1e-12);
    EXPECT_NEAR(fit.offsetD.x, 0.0, 1e-12);
    EXPECT_NEAR(fit.offsetD.y, 50.0, 1e-12);
}

TEST(CalibrationReport, LabelsTrackingDisabledAndRelativeZoom)
{
    CalibrationReport report{};
    report.roiApplied = true;
    report.units = DocumentScaleLabel::LocalCalibratedUnits;
    std::string const text = FormatCalibrationReport(report);
    EXPECT_NE(text.find("tracking=disabled"), std::string::npos);
    EXPECT_NE(text.find("not CSP %"), std::string::npos);
    EXPECT_NE(text.find("M_RD"), std::string::npos);
}
