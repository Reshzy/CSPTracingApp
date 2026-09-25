#pragma once

#include <optional>
#include <string>

namespace tracing::core {

// Double-precision homogeneous 2D transforms. Column vectors, p' = M * p.
// X-right, Y-down; positive rotation is clockwise in that convention.
// Spaces: R reference px, D document/calibrated units, S physical screen,
// O overlay-local, C capture. Storing a canonical 3x3 does not remove
// measurement drift. FlipX*FlipY and Rotate(pi) are equivalent matrices;
// this engine does not canonicalize parity to a unique UI flip-flag pair.

enum class Space
{
    R,
    D,
    S,
    O,
    C,
};

inline constexpr double kSingularEpsilon = 1e-12;

struct Vec2
{
    double x = 0.0;
    double y = 0.0;
};

struct Mat3
{
    // Column-major: m[col * 3 + row]. Identity by default.
    double m[9]{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
};

struct Transform2D
{
    Mat3 matrix{};
    Space from = Space::R;
    Space to = Space::R;
};

// Canned control-window diagnostic (identity M_RD; not connected to rendering).
// Hand-computed: pD=(10,5); pS=2*(10,5)+(100,200)=(120,210);
// pO=(120-(-1920), 210-108)=(2040,102).
inline constexpr Vec2 kCannedPR{10.0, 5.0};
inline constexpr Vec2 kCannedDocumentAnchor{0.0, 0.0};
inline constexpr Vec2 kCannedViewportAnchorS{100.0, 200.0};
inline constexpr double kCannedZoom = 2.0;
inline constexpr double kCannedRotationRadians = 0.0;
inline constexpr bool kCannedFlipX = false;
inline constexpr bool kCannedFlipY = false;
inline constexpr Vec2 kCannedOverlayOriginS{-1920.0, 108.0};
inline constexpr Vec2 kCannedExpectedPO{2040.0, 102.0};

char const* SpaceName(Space space) noexcept;

Transform2D Identity(Space from, Space to) noexcept;
Transform2D Translate(Space from, Space to, double tx, double ty) noexcept;
Transform2D UniformScale(Space from, Space to, double scale) noexcept;
Transform2D RotateClockwise(Space from, Space to, double radians) noexcept;
Transform2D Flip(Space from, Space to, bool flipX, bool flipY) noexcept;
Transform2D RotateAbout(Space space, Vec2 pivot, double radiansClockwise);
Transform2D ScaleAbout(Space space, Vec2 pivot, double scale);

Transform2D MakeReferenceToDocument() noexcept;
Transform2D MakeDocumentToScreen(
    Vec2 viewportAnchorScreen,
    double radiansClockwise,
    double zoom,
    bool flipX,
    bool flipY,
    Vec2 documentAnchor);
Transform2D MakeScreenToOverlay(Vec2 overlayPhysicalScreenOrigin) noexcept;
Transform2D MakeCaptureIdentity() noexcept;

bool IsFinite(Mat3 const& matrix) noexcept;
bool IsFinite(Transform2D const& transform) noexcept;
double LinearDeterminant(Mat3 const& matrix) noexcept;
bool IsSingular(Mat3 const& matrix) noexcept;
bool IsSingular(Transform2D const& transform) noexcept;

std::optional<Transform2D> Compose(Transform2D const& first, Transform2D const& then);
std::optional<Vec2> Apply(Transform2D const& transform, Vec2 point);
std::optional<Transform2D> TryInverse(Transform2D const& transform);
std::optional<Vec2> MapReferenceToOverlay(
    Transform2D const& mRd,
    Transform2D const& mDs,
    Transform2D const& mSo,
    Vec2 pR);

std::string FormatCannedTransformDiagnostic();

} // namespace tracing::core
