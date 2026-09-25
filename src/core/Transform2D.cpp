#include "core/Transform2D.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace tracing::core {
namespace {

Mat3 ZeroMat() noexcept
{
    Mat3 matrix{};
    for (double& value : matrix.m)
    {
        value = 0.0;
    }
    return matrix;
}

Mat3 Multiply(Mat3 const& a, Mat3 const& b) noexcept
{
    Mat3 result = ZeroMat();
    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
        {
            result.m[col * 3 + row] = a.m[0 * 3 + row] * b.m[col * 3 + 0] +
                                      a.m[1 * 3 + row] * b.m[col * 3 + 1] +
                                      a.m[2 * 3 + row] * b.m[col * 3 + 2];
        }
    }
    return result;
}

Transform2D MakeLinear(
    Space from,
    Space to,
    double a,
    double b,
    double c,
    double d,
    double tx,
    double ty) noexcept
{
    Transform2D transform{};
    transform.from = from;
    transform.to = to;
    transform.matrix.m[0] = a;
    transform.matrix.m[1] = b;
    transform.matrix.m[2] = 0.0;
    transform.matrix.m[3] = c;
    transform.matrix.m[4] = d;
    transform.matrix.m[5] = 0.0;
    transform.matrix.m[6] = tx;
    transform.matrix.m[7] = ty;
    transform.matrix.m[8] = 1.0;
    return transform;
}

void Append(std::string& text, char const* format, double value)
{
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), format, value);
    text += buffer;
}

} // namespace

char const* SpaceName(Space space) noexcept
{
    switch (space)
    {
    case Space::R:
        return "R";
    case Space::D:
        return "D";
    case Space::S:
        return "S";
    case Space::O:
        return "O";
    case Space::C:
        return "C";
    }
    return "?";
}

Transform2D Identity(Space from, Space to) noexcept
{
    return MakeLinear(from, to, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
}

Transform2D Translate(Space from, Space to, double tx, double ty) noexcept
{
    return MakeLinear(from, to, 1.0, 0.0, 0.0, 1.0, tx, ty);
}

Transform2D UniformScale(Space from, Space to, double scale) noexcept
{
    return MakeLinear(from, to, scale, 0.0, 0.0, scale, 0.0, 0.0);
}

Transform2D RotateClockwise(Space from, Space to, double radians) noexcept
{
    double const cosine = std::cos(radians);
    double const sine = std::sin(radians);
    return MakeLinear(from, to, cosine, sine, -sine, cosine, 0.0, 0.0);
}

Transform2D Flip(Space from, Space to, bool flipX, bool flipY) noexcept
{
    double const sx = flipX ? -1.0 : 1.0;
    double const sy = flipY ? -1.0 : 1.0;
    return MakeLinear(from, to, sx, 0.0, 0.0, sy, 0.0, 0.0);
}

Transform2D RotateAbout(Space space, Vec2 pivot, double radiansClockwise)
{
    Transform2D const toOrigin = Translate(space, space, -pivot.x, -pivot.y);
    Transform2D const rotate = RotateClockwise(space, space, radiansClockwise);
    Transform2D const fromOrigin = Translate(space, space, pivot.x, pivot.y);
    std::optional<Transform2D> const rotated = Compose(toOrigin, rotate);
    std::optional<Transform2D> const result =
        rotated.has_value() ? Compose(*rotated, fromOrigin) : std::nullopt;
    return result.value_or(Identity(space, space));
}

Transform2D ScaleAbout(Space space, Vec2 pivot, double scale)
{
    Transform2D const toOrigin = Translate(space, space, -pivot.x, -pivot.y);
    Transform2D const scaled = UniformScale(space, space, scale);
    Transform2D const fromOrigin = Translate(space, space, pivot.x, pivot.y);
    std::optional<Transform2D> const mid = Compose(toOrigin, scaled);
    std::optional<Transform2D> const result =
        mid.has_value() ? Compose(*mid, fromOrigin) : std::nullopt;
    return result.value_or(Identity(space, space));
}

Transform2D MakeReferenceToDocument() noexcept
{
    return Identity(Space::R, Space::D);
}

Transform2D MakeDocumentToScreen(
    Vec2 viewportAnchorScreen,
    double radiansClockwise,
    double zoom,
    bool flipX,
    bool flipY,
    Vec2 documentAnchor)
{
    Transform2D const toOrigin =
        Translate(Space::D, Space::D, -documentAnchor.x, -documentAnchor.y);
    Transform2D const flip = Flip(Space::D, Space::D, flipX, flipY);
    Transform2D const scale = UniformScale(Space::D, Space::D, zoom);
    Transform2D const rotate = RotateClockwise(Space::D, Space::D, radiansClockwise);
    Transform2D const toScreen =
        Translate(Space::D, Space::S, viewportAnchorScreen.x, viewportAnchorScreen.y);

    std::optional<Transform2D> composed = Compose(toOrigin, flip);
    if (composed.has_value())
    {
        composed = Compose(*composed, scale);
    }
    if (composed.has_value())
    {
        composed = Compose(*composed, rotate);
    }
    if (composed.has_value())
    {
        composed = Compose(*composed, toScreen);
    }
    return composed.value_or(Identity(Space::D, Space::S));
}

Transform2D MakeScreenToOverlay(Vec2 overlayPhysicalScreenOrigin) noexcept
{
    return Translate(
        Space::S,
        Space::O,
        -overlayPhysicalScreenOrigin.x,
        -overlayPhysicalScreenOrigin.y);
}

Transform2D MakeCaptureIdentity() noexcept
{
    return Identity(Space::C, Space::C);
}

bool IsFinite(Mat3 const& matrix) noexcept
{
    for (double const value : matrix.m)
    {
        if (!std::isfinite(value))
        {
            return false;
        }
    }
    return true;
}

bool IsFinite(Transform2D const& transform) noexcept
{
    return IsFinite(transform.matrix);
}

double LinearDeterminant(Mat3 const& matrix) noexcept
{
    return matrix.m[0] * matrix.m[4] - matrix.m[3] * matrix.m[1];
}

bool IsSingular(Mat3 const& matrix) noexcept
{
    if (!IsFinite(matrix))
    {
        return true;
    }
    return std::fabs(LinearDeterminant(matrix)) < kSingularEpsilon;
}

bool IsSingular(Transform2D const& transform) noexcept
{
    return IsSingular(transform.matrix);
}

std::optional<Transform2D> Compose(Transform2D const& first, Transform2D const& then)
{
    if (first.to != then.from)
    {
        return std::nullopt;
    }

    Transform2D result{};
    result.from = first.from;
    result.to = then.to;
    result.matrix = Multiply(then.matrix, first.matrix);
    return result;
}

std::optional<Vec2> Apply(Transform2D const& transform, Vec2 point)
{
    if (!IsFinite(transform))
    {
        return std::nullopt;
    }

    double const x = point.x;
    double const y = point.y;
    double const w = transform.matrix.m[2] * x + transform.matrix.m[5] * y + transform.matrix.m[8];
    if (!std::isfinite(w) || std::fabs(w) < kSingularEpsilon)
    {
        return std::nullopt;
    }

    Vec2 mapped{};
    mapped.x = (transform.matrix.m[0] * x + transform.matrix.m[3] * y + transform.matrix.m[6]) / w;
    mapped.y = (transform.matrix.m[1] * x + transform.matrix.m[4] * y + transform.matrix.m[7]) / w;
    if (!std::isfinite(mapped.x) || !std::isfinite(mapped.y))
    {
        return std::nullopt;
    }
    return mapped;
}

std::optional<Transform2D> TryInverse(Transform2D const& transform)
{
    if (!IsFinite(transform) || IsSingular(transform))
    {
        return std::nullopt;
    }

    double const det = LinearDeterminant(transform.matrix);
    double const invDet = 1.0 / det;
    double const a = transform.matrix.m[0];
    double const b = transform.matrix.m[1];
    double const c = transform.matrix.m[3];
    double const d = transform.matrix.m[4];
    double const tx = transform.matrix.m[6];
    double const ty = transform.matrix.m[7];

    double const ia = d * invDet;
    double const ib = -b * invDet;
    double const ic = -c * invDet;
    double const id = a * invDet;
    double const itx = -(ia * tx + ic * ty);
    double const ity = -(ib * tx + id * ty);

    Transform2D inverse = MakeLinear(transform.to, transform.from, ia, ib, ic, id, itx, ity);
    if (!IsFinite(inverse) || IsSingular(inverse))
    {
        return std::nullopt;
    }
    return inverse;
}

std::optional<Vec2> MapReferenceToOverlay(
    Transform2D const& mRd,
    Transform2D const& mDs,
    Transform2D const& mSo,
    Vec2 pR)
{
    if (mRd.from != Space::R || mRd.to != Space::D)
    {
        return std::nullopt;
    }
    if (mDs.from != Space::D || mDs.to != Space::S)
    {
        return std::nullopt;
    }
    if (mSo.from != Space::S || mSo.to != Space::O)
    {
        return std::nullopt;
    }

    std::optional<Vec2> const pD = Apply(mRd, pR);
    if (!pD.has_value())
    {
        return std::nullopt;
    }
    std::optional<Vec2> const pS = Apply(mDs, *pD);
    if (!pS.has_value())
    {
        return std::nullopt;
    }
    return Apply(mSo, *pS);
}

std::string FormatCannedTransformDiagnostic()
{
    Transform2D const mRd = MakeReferenceToDocument();
    Transform2D const mDs = MakeDocumentToScreen(
        kCannedViewportAnchorS,
        kCannedRotationRadians,
        kCannedZoom,
        kCannedFlipX,
        kCannedFlipY,
        kCannedDocumentAnchor);
    Transform2D const mSo = MakeScreenToOverlay(kCannedOverlayOriginS);
    std::optional<Vec2> const pO = MapReferenceToOverlay(mRd, mDs, mSo, kCannedPR);

    std::optional<Transform2D> const mRs = Compose(mRd, mDs);
    std::optional<Transform2D> const mRo =
        mRs.has_value() ? Compose(*mRs, mSo) : std::nullopt;
    std::optional<Transform2D> const inverse =
        mRo.has_value() ? TryInverse(*mRo) : std::nullopt;
    double roundTrip = 0.0;
    bool roundTripOk = false;
    if (pO.has_value() && inverse.has_value())
    {
        std::optional<Vec2> const back = Apply(*inverse, *pO);
        if (back.has_value())
        {
            double const dx = back->x - kCannedPR.x;
            double const dy = back->y - kCannedPR.y;
            roundTrip = std::hypot(dx, dy);
            roundTripOk = true;
        }
    }

    std::string text;
    text += "numerical only; not connected to image placement or tracking\r\n";
    text += "canonical 3x3 storage does not remove measurement drift\r\n";
    text += "column-vector X-right Y-down; positive rotation clockwise\r\n";
    text += "pO = M_SO * M_DS * M_RD * pR (M_RD kept separate from M_DS)\r\n";
    text += "M_RD Identity ";
    text += SpaceName(mRd.from);
    text += "->";
    text += SpaceName(mRd.to);
    text += " finite=";
    text += IsFinite(mRd) ? "yes" : "no";
    text += " singular=";
    text += IsSingular(mRd) ? "yes" : "no";
    text += "\r\nM_DS T(100,200)*S(2) D->S finite=";
    text += IsFinite(mDs) ? "yes" : "no";
    text += " singular=";
    text += IsSingular(mDs) ? "yes" : "no";
    text += "\r\nM_SO T(-overlayOrigin) origin=(-1920,108) S->O finite=";
    text += IsFinite(mSo) ? "yes" : "no";
    text += " singular=";
    text += IsSingular(mSo) ? "yes" : "no";
    text += "\r\npR=(10,5) expected pO=(2040,102) actual pO=";
    if (pO.has_value())
    {
        text += "(";
        Append(text, "%.9g", pO->x);
        text += ",";
        Append(text, "%.9g", pO->y);
        text += ")";
    }
    else
    {
        text += "(unmapped)";
    }
    text += "\r\ninverse round-trip |pR'-pR|=";
    if (roundTripOk)
    {
        Append(text, "%.3e", roundTrip);
    }
    else
    {
        text += "n/a";
    }
    text += "\r\nSpace C capture identity present (no calibrated M_CS in this step)";
    Transform2D const capture = MakeCaptureIdentity();
    text += " C->";
    text += SpaceName(capture.to);
    text += " finite=";
    text += IsFinite(capture) ? "yes" : "no";
    return text;
}

} // namespace tracing::core
