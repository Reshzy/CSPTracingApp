#include "SequenceGenerator.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace tracing::replay {
namespace {

using tracing::core::Compose;
using tracing::core::Flip;
using tracing::core::Identity;
using tracing::core::RotateAbout;
using tracing::core::ScaleAbout;
using tracing::core::Space;
using tracing::core::Transform2D;
using tracing::core::Translate;
using tracing::core::Vec2;

constexpr double kPi = std::numbers::pi_v<double>;

Transform2D MustCompose(Transform2D const& first, Transform2D const& then)
{
    return Compose(first, then).value_or(first);
}

cv::Mat MakeTexturedGray(std::uint32_t seed)
{
    cv::Mat gray(kReplayHeight, kReplayWidth, CV_8UC1);
    std::uint32_t rng = seed;
    auto next = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return rng;
    };
    for (int y = 0; y < kReplayHeight; ++y)
    {
        for (int x = 0; x < kReplayWidth; ++x)
        {
            std::uint32_t const noise = next() >> 24;
            int const checker = ((x / 10) ^ (y / 10)) & 1;
            int const value = static_cast<int>(noise) + checker * 70 + ((x * 17 + y * 11) & 47);
            gray.at<std::uint8_t>(y, x) = static_cast<std::uint8_t>(std::clamp(value, 0, 255));
        }
    }
    cv::circle(gray, cv::Point(48, 42), 18, cv::Scalar(255), cv::FILLED);
    cv::circle(gray, cv::Point(kReplayWidth - 56, 46), 14, cv::Scalar(16), cv::FILLED);
    cv::rectangle(gray, cv::Point(28, kReplayHeight - 72), cv::Point(96, kReplayHeight - 22),
                  cv::Scalar(230), cv::FILLED);
    cv::rectangle(gray, cv::Point(150, 96), cv::Point(214, 110), cv::Scalar(8), cv::FILLED);
    cv::rectangle(gray, cv::Point(150, 96), cv::Point(164, 176), cv::Scalar(8), cv::FILLED);
    cv::line(gray, cv::Point(18, 18), cv::Point(kReplayWidth - 24, 78), cv::Scalar(250), 3);
    cv::line(gray, cv::Point(40, kReplayHeight - 18), cv::Point(kReplayWidth - 30, kReplayHeight - 90),
             cv::Scalar(12), 3);
    return gray;
}

cv::Mat ToOpenCvAffine(Transform2D const& transform)
{
    cv::Mat matrix(2, 3, CV_64F);
    matrix.at<double>(0, 0) = transform.matrix.m[0];
    matrix.at<double>(0, 1) = transform.matrix.m[3];
    matrix.at<double>(0, 2) = transform.matrix.m[6];
    matrix.at<double>(1, 0) = transform.matrix.m[1];
    matrix.at<double>(1, 1) = transform.matrix.m[4];
    matrix.at<double>(1, 2) = transform.matrix.m[7];
    return matrix;
}

cv::Mat WarpGray(cv::Mat const& source, Transform2D const& transform)
{
    cv::Mat destination;
    cv::warpAffine(source, destination, ToOpenCvAffine(transform), source.size(), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar(128));
    return destination;
}

void PaintStroke(cv::Mat& gray)
{
    cv::polylines(
        gray,
        std::vector<std::vector<cv::Point>>{{cv::Point(20, 30), cv::Point(80, 90), cv::Point(40, 180),
                                            cv::Point(120, 140), cv::Point(200, 210)}},
        false,
        cv::Scalar(255),
        14,
        cv::LINE_8);
}

void PackBgra(cv::Mat const& gray, ReplayFrame& frame)
{
    cv::Mat bgra;
    cv::cvtColor(gray, bgra, cv::COLOR_GRAY2BGRA);
    frame.width = bgra.cols;
    frame.height = bgra.rows;
    frame.stride = bgra.cols * 4;
    std::size_t const bytes =
        static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height);
    frame.bgra.resize(bytes);
    if (bgra.isContinuous() && bgra.elemSize() == 4)
    {
        std::memcpy(frame.bgra.data(), bgra.data, bytes);
        return;
    }
    for (int y = 0; y < bgra.rows; ++y)
    {
        std::memcpy(
            frame.bgra.data() + static_cast<std::size_t>(y * frame.stride),
            bgra.ptr(y),
            static_cast<std::size_t>(bgra.cols * 4));
    }
}

void DecomposeWarp(Transform2D const& transform, ReplayFrame& frame)
{
    double const a = transform.matrix.m[0];
    double const c = transform.matrix.m[1];
    frame.tx = transform.matrix.m[6];
    frame.ty = transform.matrix.m[7];
    frame.uniformScale = std::hypot(a, c);
    frame.radiansClockwise = std::atan2(c, a);
    frame.warp = transform;
}

Transform2D CenterFlipX()
{
    Vec2 const center{kReplayWidth * 0.5, kReplayHeight * 0.5};
    Transform2D const toOrigin = Translate(Space::C, Space::C, -center.x, -center.y);
    Transform2D const flip = Flip(Space::C, Space::C, true, false);
    Transform2D const fromOrigin = Translate(Space::C, Space::C, center.x, center.y);
    return MustCompose(MustCompose(toOrigin, flip), fromOrigin);
}

ReplayFrame MakeFrame(
    std::uint64_t sequence,
    ReplayEvent event,
    cv::Mat const& gray,
    Transform2D const& warp,
    bool settledMaster,
    bool flipX = false)
{
    ReplayFrame frame{};
    frame.sequence = sequence;
    frame.event = event;
    frame.settledMaster = settledMaster;
    frame.flipX = flipX;
    DecomposeWarp(warp, frame);
    PackBgra(gray, frame);
    return frame;
}

} // namespace

char const* ReplayEventName(ReplayEvent event) noexcept
{
    switch (event)
    {
    case ReplayEvent::Keyframe:
        return "keyframe";
    case ReplayEvent::Pan:
        return "pan";
    case ReplayEvent::Scale:
        return "scale";
    case ReplayEvent::Rotate:
        return "rotate";
    case ReplayEvent::Combined:
        return "combined";
    case ReplayEvent::Stroke:
        return "stroke";
    case ReplayEvent::Missing:
        return "missing";
    case ReplayEvent::GapPan:
        return "gap-pan";
    case ReplayEvent::Blank:
        return "blank";
    case ReplayEvent::Reacquire:
        return "reacquire";
    case ReplayEvent::FlipX:
        return "flip-x";
    case ReplayEvent::Identity:
        return "identity";
    }
    return "unknown";
}

std::vector<Vec2> ReplayLandmarks()
{
    std::vector<Vec2> points;
    points.reserve(25);
    for (int y = 40; y <= 200; y += 40)
    {
        for (int x = 40; x <= 280; x += 60)
        {
            points.push_back(Vec2{static_cast<double>(x), static_cast<double>(y)});
        }
    }
    return points;
}

std::vector<ReplayFrame> BuildSequence(std::uint32_t seed)
{
    cv::Mat const prior = MakeTexturedGray(seed);
    Vec2 const center{kReplayWidth * 0.5, kReplayHeight * 0.5};
    Transform2D const identity = Identity(Space::C, Space::C);
    Transform2D const pan = Translate(Space::C, Space::C, 8.0, 5.0);
    Transform2D const scale = ScaleAbout(Space::C, center, 1.12);
    Transform2D const rotate = RotateAbout(Space::C, Vec2{118.0, 86.0}, 8.0 * kPi / 180.0);
    Transform2D const combined = MustCompose(
        MustCompose(ScaleAbout(Space::C, center, 1.12),
                    RotateAbout(Space::C, Vec2{140.0, 100.0}, 8.0 * kPi / 180.0)),
        Translate(Space::C, Space::C, 7.0, -5.0));
    Transform2D const flipX = CenterFlipX();

    std::vector<ReplayFrame> frames;
    frames.reserve(18);

    frames.push_back(MakeFrame(1, ReplayEvent::Keyframe, prior, identity, false));
    frames.push_back(MakeFrame(2, ReplayEvent::Pan, WarpGray(prior, pan), pan, true));
    frames.push_back(MakeFrame(3, ReplayEvent::Scale, WarpGray(prior, scale), scale, true));
    frames.push_back(MakeFrame(4, ReplayEvent::Rotate, WarpGray(prior, rotate), rotate, true));
    frames.push_back(MakeFrame(5, ReplayEvent::Combined, WarpGray(prior, combined), combined, true));

    cv::Mat strokeGray = WarpGray(prior, pan);
    PaintStroke(strokeGray);
    ReplayFrame stroke = MakeFrame(6, ReplayEvent::Stroke, strokeGray, pan, true);
    stroke.stroke = true;
    frames.push_back(std::move(stroke));

    ReplayFrame missing{};
    missing.sequence = 7;
    missing.event = ReplayEvent::Missing;
    missing.missing = true;
    missing.warp = pan;
    DecomposeWarp(pan, missing);
    frames.push_back(std::move(missing));

    frames.push_back(MakeFrame(8, ReplayEvent::GapPan, WarpGray(prior, pan), pan, true));

    ReplayFrame blank{};
    blank.sequence = 9;
    blank.event = ReplayEvent::Blank;
    blank.blank = true;
    blank.warp = pan;
    DecomposeWarp(pan, blank);
    cv::Mat blankGray(kReplayHeight, kReplayWidth, CV_8UC1, cv::Scalar(128));
    PackBgra(blankGray, blank);
    frames.push_back(std::move(blank));

    cv::Mat const panGray = WarpGray(prior, pan);
    frames.push_back(MakeFrame(10, ReplayEvent::Reacquire, panGray, pan, false));
    frames.push_back(MakeFrame(11, ReplayEvent::Reacquire, panGray, pan, false));
    frames.push_back(MakeFrame(12, ReplayEvent::Reacquire, panGray, pan, false));

    cv::Mat const flipGray = WarpGray(prior, flipX);
    frames.push_back(MakeFrame(13, ReplayEvent::FlipX, flipGray, flipX, false, true));
    frames.push_back(MakeFrame(14, ReplayEvent::FlipX, flipGray, flipX, false, true));
    frames.push_back(MakeFrame(15, ReplayEvent::FlipX, flipGray, flipX, false, true));
    frames.push_back(MakeFrame(16, ReplayEvent::FlipX, flipGray, flipX, false, true));

    frames.push_back(MakeFrame(17, ReplayEvent::Identity, prior, identity, false));
    frames.push_back(MakeFrame(18, ReplayEvent::Identity, prior, identity, false));
    return frames;
}

} // namespace tracing::replay
