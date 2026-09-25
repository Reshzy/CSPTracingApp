#include <gtest/gtest.h>

#include "core/Transform2D.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <string>

namespace {

using tracing::core::Apply;
using tracing::core::Compose;
using tracing::core::Flip;
using tracing::core::Identity;
using tracing::core::IsFinite;
using tracing::core::IsSingular;
using tracing::core::kCannedExpectedPO;
using tracing::core::kCannedOverlayOriginS;
using tracing::core::kCannedPR;
using tracing::core::kCannedViewportAnchorS;
using tracing::core::kCannedZoom;
using tracing::core::MakeCaptureIdentity;
using tracing::core::MakeDocumentToScreen;
using tracing::core::MakeReferenceToDocument;
using tracing::core::MakeScreenToOverlay;
using tracing::core::MapReferenceToOverlay;
using tracing::core::RotateAbout;
using tracing::core::RotateClockwise;
using tracing::core::Space;
using tracing::core::Transform2D;
using tracing::core::Translate;
using tracing::core::TryInverse;
using tracing::core::UniformScale;
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

} // namespace

TEST(TransformKnownPoints, TranslateMapsIndependentPoint)
{
    Transform2D const t = Translate(Space::R, Space::R, 3.0, 4.0);
    ExpectVecNear(MustApply(t, Vec2{1.0, 2.0}), 4.0, 6.0);
}

TEST(TransformKnownPoints, UniformScaleMapsIndependentPoint)
{
    Transform2D const s = UniformScale(Space::R, Space::R, 2.0);
    ExpectVecNear(MustApply(s, Vec2{3.0, 4.0}), 6.0, 8.0);
}

TEST(TransformKnownPoints, Rotate90ClockwiseYDown)
{
    Transform2D const r = RotateClockwise(Space::R, Space::R, kPi / 2.0);
    ExpectVecNear(MustApply(r, Vec2{1.0, 0.0}), 0.0, 1.0);
    ExpectVecNear(MustApply(r, Vec2{0.0, 1.0}), -1.0, 0.0);
}

TEST(TransformOrder, TranslateThenRotateDoesNotCommuteWithRotateThenTranslate)
{
    Transform2D const t = Translate(Space::R, Space::R, 10.0, 0.0);
    Transform2D const r = RotateClockwise(Space::R, Space::R, kPi / 2.0);
    std::optional<Transform2D> const tr = Compose(t, r);
    std::optional<Transform2D> const rt = Compose(r, t);
    ASSERT_TRUE(tr.has_value());
    ASSERT_TRUE(rt.has_value());

    Vec2 const p{1.0, 0.0};
    Vec2 const afterTR = MustApply(*tr, p);
    Vec2 const afterRT = MustApply(*rt, p);
    ExpectVecNear(afterTR, 0.0, 11.0);
    ExpectVecNear(afterRT, 10.0, 1.0);
    EXPECT_GT(std::hypot(afterTR.x - afterRT.x, afterTR.y - afterRT.y), 1.0);
}

TEST(TransformPivots, Rotate90ClockwiseAboutOffCenterPoint)
{
    Transform2D const r = RotateAbout(Space::R, Vec2{10.0, 20.0}, kPi / 2.0);
    ExpectVecNear(MustApply(r, Vec2{10.0, 20.0}), 10.0, 20.0);
    ExpectVecNear(MustApply(r, Vec2{11.0, 20.0}), 10.0, 21.0);
    ExpectVecNear(MustApply(r, Vec2{10.0, 21.0}), 9.0, 20.0);
}

TEST(TransformPivots, UniformScaleAboutOffCenterPoint)
{
    Transform2D const s = tracing::core::ScaleAbout(Space::R, Vec2{10.0, 20.0}, 2.0);
    ExpectVecNear(MustApply(s, Vec2{10.0, 20.0}), 10.0, 20.0);
    ExpectVecNear(MustApply(s, Vec2{12.0, 20.0}), 14.0, 20.0);
}

TEST(TransformFlips, FlipXAndFlipYAreIndependent)
{
    Transform2D const fx = Flip(Space::R, Space::R, true, false);
    Transform2D const fy = Flip(Space::R, Space::R, false, true);
    ExpectVecNear(MustApply(fx, Vec2{5.0, 3.0}), -5.0, 3.0);
    ExpectVecNear(MustApply(fy, Vec2{5.0, 3.0}), 5.0, -3.0);
}

TEST(TransformParity, FlipXThenFlipYEqualsRotatePiOnKnownPoints)
{
    Transform2D const fx = Flip(Space::R, Space::R, true, false);
    Transform2D const fy = Flip(Space::R, Space::R, false, true);
    std::optional<Transform2D> const both = Compose(fx, fy);
    Transform2D const rot = RotateClockwise(Space::R, Space::R, kPi);
    ASSERT_TRUE(both.has_value());

    Vec2 const samples[] = {{5.0, 3.0}, {0.0, 0.0}, {-2.0, 7.5}, {1.0, -4.0}};
    for (Vec2 const p : samples)
    {
        Vec2 const fromFlips = MustApply(*both, p);
        Vec2 const fromRot = MustApply(rot, p);
        EXPECT_NEAR(fromFlips.x, fromRot.x, 1e-9);
        EXPECT_NEAR(fromFlips.y, fromRot.y, 1e-9);
    }
    ExpectVecNear(MustApply(*both, Vec2{5.0, 3.0}), -5.0, -3.0);
}

TEST(TransformScreen, NegativeMonitorOriginMapsToOverlayLocal)
{
    Transform2D const mSo = MakeScreenToOverlay(Vec2{-1920.0, 108.0});
    EXPECT_EQ(mSo.from, Space::S);
    EXPECT_EQ(mSo.to, Space::O);
    ExpectVecNear(MustApply(mSo, Vec2{0.0, 108.0}), 1920.0, 0.0);
}

TEST(TransformInverse, RoundTripErrorBelowOneNanoForInvertibleAffine)
{
    Transform2D const t = Translate(Space::R, Space::R, -40.0, 25.5);
    Transform2D const s = UniformScale(Space::R, Space::R, 1.5);
    Transform2D const r = RotateClockwise(Space::R, Space::R, 0.3);
    std::optional<Transform2D> const ts = Compose(t, s);
    ASSERT_TRUE(ts.has_value());
    std::optional<Transform2D> const affine = Compose(*ts, r);
    ASSERT_TRUE(affine.has_value());
    std::optional<Transform2D> const inverse = TryInverse(*affine);
    ASSERT_TRUE(inverse.has_value());

    Vec2 const p{12.25, -8.5};
    Vec2 const mapped = MustApply(*affine, p);
    Vec2 const back = MustApply(*inverse, mapped);
    EXPECT_LT(std::hypot(back.x - p.x, back.y - p.y), 1e-9);
}

TEST(TransformValidity, NonFiniteRejectedAndZeroScaleIsSingular)
{
    Transform2D nanTransform = Identity(Space::R, Space::R);
    nanTransform.matrix.m[0] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(IsFinite(nanTransform));
    EXPECT_FALSE(Apply(nanTransform, Vec2{1.0, 1.0}).has_value());
    EXPECT_FALSE(TryInverse(nanTransform).has_value());

    Transform2D infTransform = Identity(Space::R, Space::R);
    infTransform.matrix.m[6] = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(IsFinite(infTransform));

    Transform2D const zeroScale = UniformScale(Space::R, Space::R, 0.0);
    EXPECT_TRUE(IsFinite(zeroScale));
    EXPECT_TRUE(IsSingular(zeroScale));
    EXPECT_FALSE(TryInverse(zeroScale).has_value());
}

TEST(TransformSpaces, ComposingMrdWithMsoFailsAndCaptureIdentityExists)
{
    Transform2D const mRd = MakeReferenceToDocument();
    Transform2D const mSo = MakeScreenToOverlay(kCannedOverlayOriginS);
    EXPECT_FALSE(Compose(mRd, mSo).has_value());

    Transform2D const mDs = MakeDocumentToScreen(
        kCannedViewportAnchorS,
        0.0,
        kCannedZoom,
        false,
        false,
        Vec2{0.0, 0.0});
    EXPECT_EQ(mRd.from, Space::R);
    EXPECT_EQ(mRd.to, Space::D);
    EXPECT_EQ(mDs.from, Space::D);
    EXPECT_EQ(mDs.to, Space::S);
    EXPECT_NE(mRd.matrix.m[6], mDs.matrix.m[6]);

    Transform2D const capture = MakeCaptureIdentity();
    EXPECT_EQ(capture.from, Space::C);
    EXPECT_EQ(capture.to, Space::C);
    ExpectVecNear(MustApply(capture, Vec2{3.0, 4.0}), 3.0, 4.0);
}

TEST(TransformChain, IdentityMrdKnownMdsNegativeOriginMso)
{
    Transform2D const mRd = MakeReferenceToDocument();
    Transform2D const mDs = MakeDocumentToScreen(
        kCannedViewportAnchorS,
        0.0,
        kCannedZoom,
        false,
        false,
        Vec2{0.0, 0.0});
    Transform2D const mSo = MakeScreenToOverlay(kCannedOverlayOriginS);
    std::optional<Vec2> const pO = MapReferenceToOverlay(mRd, mDs, mSo, kCannedPR);
    ASSERT_TRUE(pO.has_value());
    ExpectVecNear(*pO, kCannedExpectedPO.x, kCannedExpectedPO.y);

    Transform2D const swappedRd = Translate(Space::R, Space::D, 50.0, 0.0);
    std::optional<Vec2> const moved =
        MapReferenceToOverlay(swappedRd, mDs, mSo, kCannedPR);
    ASSERT_TRUE(moved.has_value());
    EXPECT_NEAR(mDs.matrix.m[6], 100.0, 1e-12);
    EXPECT_NEAR(mDs.matrix.m[7], 200.0, 1e-12);
    EXPECT_GT(std::fabs(moved->x - pO->x), 1.0);
}

TEST(TransformChain, MapRejectsMismatchedSpaces)
{
    Transform2D const mRd = MakeReferenceToDocument();
    Transform2D const mDs = MakeDocumentToScreen(
        kCannedViewportAnchorS,
        0.0,
        kCannedZoom,
        false,
        false,
        Vec2{0.0, 0.0});
    Transform2D const mSo = MakeScreenToOverlay(kCannedOverlayOriginS);
    EXPECT_FALSE(MapReferenceToOverlay(mDs, mDs, mSo, kCannedPR).has_value());
    EXPECT_FALSE(MapReferenceToOverlay(mRd, mRd, mSo, kCannedPR).has_value());
    EXPECT_FALSE(MapReferenceToOverlay(mRd, mDs, mRd, kCannedPR).has_value());
}

TEST(TransformDiagnostic, CannedTextContainsKnownOverlayPoint)
{
    std::string const text = tracing::core::FormatCannedTransformDiagnostic();
    EXPECT_NE(text.find("actual pO=(2040,102)"), std::string::npos);
    EXPECT_NE(text.find("numerical only"), std::string::npos);
    EXPECT_NE(text.find("does not remove measurement drift"), std::string::npos);
}
