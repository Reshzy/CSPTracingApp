#include "tracking/NavigatorObserver.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace tracing::tracking {
namespace {

constexpr double kPi = 3.14159265358979323846;

bool IsFiniteValue(double value) noexcept
{
    return std::isfinite(value);
}

double Clamp01(double value) noexcept
{
    if (value < 0.0)
    {
        return 0.0;
    }
    if (value > 1.0)
    {
        return 1.0;
    }
    return value;
}

double WrapHalfPi(double radians) noexcept
{
    while (radians > kPi * 0.5)
    {
        radians -= kPi;
    }
    while (radians <= -kPi * 0.5)
    {
        radians += kPi;
    }
    return radians;
}

cv::Mat ToGray(NavigatorView const& view)
{
    int const type = view.format == NavigatorPixelFormat::Bgra32 ? CV_8UC4 : CV_8UC1;
    cv::Mat wrapped(
        view.height,
        view.width,
        type,
        const_cast<std::uint8_t*>(view.data),
        static_cast<std::size_t>(view.stride));
    cv::Mat gray;
    if (view.format == NavigatorPixelFormat::Bgra32)
    {
        cv::cvtColor(wrapped, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        gray = wrapped.clone();
    }
    return gray;
}

double GrayVariance(cv::Mat const& gray)
{
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(gray, mean, stddev);
    return stddev[0] * stddev[0];
}

struct IndicatorCandidate
{
    cv::RotatedRect box{};
    double rectangularity = 0.0;
    double area = 0.0;
    double score = 0.0;
    bool occluded = false;
};

bool BoxTouchesBorder(cv::RotatedRect const& box, int width, int height, double margin) noexcept
{
    cv::Point2f pts[4]{};
    box.points(pts);
    for (int i = 0; i < 4; ++i)
    {
        if (pts[i].x < margin || pts[i].y < margin || pts[i].x > static_cast<double>(width - 1) - margin ||
            pts[i].y > static_cast<double>(height - 1) - margin)
        {
            return true;
        }
    }
    cv::Rect const bounds = box.boundingRect();
    return bounds.x <= 0 || bounds.y <= 0 || bounds.x + bounds.width >= width ||
           bounds.y + bounds.height >= height;
}

double LongerEdgeRadiansClockwise(cv::RotatedRect const& box) noexcept
{
    cv::Point2f pts[4]{};
    box.points(pts);
    double bestLen = -1.0;
    double angle = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        cv::Point2f const delta = pts[(i + 1) % 4] - pts[i];
        double const len = std::hypot(delta.x, delta.y);
        if (len > bestLen)
        {
            bestLen = len;
            angle = std::atan2(delta.y, delta.x);
        }
    }
    return WrapHalfPi(angle);
}

void CollectCandidates(
    cv::Mat const& binary,
    int width,
    int height,
    NavigatorObserverOptions const& options,
    std::vector<IndicatorCandidate>& out)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    double const imageArea = static_cast<double>(width) * static_cast<double>(height);
    double const minArea = options.minAreaFraction * imageArea;
    double const maxArea = options.maxAreaFraction * imageArea;

    for (auto const& contour : contours)
    {
        if (contour.size() < 4)
        {
            continue;
        }
        double const contourArea = std::fabs(cv::contourArea(contour));
        if (!IsFiniteValue(contourArea) || contourArea < minArea || contourArea > maxArea)
        {
            continue;
        }

        std::vector<cv::Point> approx;
        cv::approxPolyDP(contour, approx, 0.04 * cv::arcLength(contour, true), true);
        if (approx.size() < 4 || approx.size() > 8)
        {
            continue;
        }

        cv::RotatedRect const box = cv::minAreaRect(contour);
        double const bw = static_cast<double>(box.size.width);
        double const bh = static_cast<double>(box.size.height);
        double const boxArea = bw * bh;
        if (!IsFiniteValue(boxArea) || boxArea <= 1.0)
        {
            continue;
        }

        double const aspect = bw >= bh ? (bh / bw) : (bw / bh);
        if (aspect < options.minAspect || (1.0 / std::max(aspect, 1.0e-9)) > options.maxAspect)
        {
            continue;
        }

        double const rectangularity = contourArea / boxArea;
        if (rectangularity < options.minRectangularity)
        {
            continue;
        }

        IndicatorCandidate candidate{};
        candidate.box = box;
        candidate.rectangularity = rectangularity;
        candidate.area = contourArea;
        candidate.occluded = BoxTouchesBorder(box, width, height, options.edgeMarginPx);
        candidate.score = rectangularity * std::log(contourArea + 1.0);
        out.push_back(candidate);
    }
}

std::vector<IndicatorCandidate> DetectIndicators(
    cv::Mat const& gray,
    NavigatorObserverOptions const& options)
{
    std::vector<IndicatorCandidate> candidates;
    int const width = gray.cols;
    int const height = gray.rows;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));

    cv::Mat otsu;
    cv::threshold(gray, otsu, 0.0, 255.0, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat otsuInv;
    cv::bitwise_not(otsu, otsuInv);
    CollectCandidates(otsu, width, height, options, candidates);
    CollectCandidates(otsuInv, width, height, options, candidates);

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0.0);
    cv::Mat residual;
    cv::absdiff(gray, blurred, residual);
    cv::Mat binary;
    cv::threshold(residual, binary, 0.0, 255.0, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
    CollectCandidates(binary, width, height, options, candidates);

    if (candidates.empty())
    {
        cv::Mat edges;
        cv::Canny(gray, edges, options.cannyLow, options.cannyHigh);
        cv::dilate(edges, edges, kernel);
        CollectCandidates(edges, width, height, options, candidates);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](IndicatorCandidate const& a, IndicatorCandidate const& b) { return a.score > b.score; });

    std::vector<IndicatorCandidate> unique;
    for (IndicatorCandidate const& candidate : candidates)
    {
        bool duplicate = false;
        for (IndicatorCandidate const& kept : unique)
        {
            double const dx = candidate.box.center.x - kept.box.center.x;
            double const dy = candidate.box.center.y - kept.box.center.y;
            if (std::hypot(dx, dy) < 6.0)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            unique.push_back(candidate);
        }
    }
    return unique;
}

void AppendDouble(std::string& text, char const* format, double value)
{
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), format, value);
    text += buffer;
}

} // namespace

char const* FormatNavigatorSource(NavigatorSource source) noexcept
{
    switch (source)
    {
    case NavigatorSource::Disabled:
        return "disabled";
    case NavigatorSource::UncalibratedRoi:
        return "uncalibrated-roi";
    case NavigatorSource::Missing:
        return "missing";
    case NavigatorSource::Occluded:
        return "occluded";
    case NavigatorSource::Ambiguous:
        return "ambiguous";
    case NavigatorSource::Observed:
        return "observed";
    }
    return "unknown";
}

char const* FormatNavigatorReject(NavigatorReject reject) noexcept
{
    switch (reject)
    {
    case NavigatorReject::Ok:
        return "ok";
    case NavigatorReject::Disabled:
        return "disabled";
    case NavigatorReject::NoRoi:
        return "no-roi";
    case NavigatorReject::DegenerateSize:
        return "degenerate-size";
    case NavigatorReject::BlankOrNoIndicator:
        return "blank-or-no-indicator";
    case NavigatorReject::Occluded:
        return "occluded";
    case NavigatorReject::Ambiguous:
        return "ambiguous";
    case NavigatorReject::NonFinite:
        return "non-finite";
    }
    return "unknown";
}

std::string FormatNavigatorObservation(NavigatorObservation const& observation)
{
    std::string text = "nav src=";
    text += FormatNavigatorSource(observation.source);
    text += " reject=";
    text += FormatNavigatorReject(observation.reject);
    text += " seq=";
    text += std::to_string(observation.sequence);
    text += " ticks=";
    text += std::to_string(observation.captureTicks);
    text += " trans=";
    text += observation.hasTranslation ? "yes" : "no";
    text += " scale=";
    text += observation.hasRelativeScale ? "yes" : "no";
    text += " rot=";
    text += observation.hasRotation ? "yes" : "no";
    text += " parity=no";
    text += " u=";
    AppendDouble(text, "%.3f", observation.centerU);
    text += " v=";
    AppendDouble(text, "%.3f", observation.centerV);
    text += " wu=";
    AppendDouble(text, "%.3f", observation.sizeU);
    text += " hv=";
    AppendDouble(text, "%.3f", observation.sizeV);
    text += " rotRad=";
    AppendDouble(text, "%.4f", observation.radiansClockwise);
    text += " conf=";
    AppendDouble(text, "%.3f", observation.confidence);
    text += " doc=";
    text += observation.hasDocumentPosition ? "yes" : "no";
    if (observation.hasDocumentPosition)
    {
        text += " x=";
        AppendDouble(text, "%.3f", observation.documentX);
        text += " y=";
        AppendDouble(text, "%.3f", observation.documentY);
        text += " units=";
        text += core::FormatDocumentScaleLabel(observation.mapping.units);
    }
    text += " zoom%=";
    if (observation.hasZoomPercent)
    {
        AppendDouble(text, "%.1f", observation.zoomPercent);
        text += " (relative not CSP %)";
    }
    else
    {
        text += "no (not CSP %)";
    }
    text += " mapping=";
    text += observation.mapping.applied ? "yes" : "no";
    text += " candidates=";
    text += std::to_string(observation.candidateCount);
    text += " roi=";
    AppendDouble(text, "%.0f", observation.roi.x);
    text += ",";
    AppendDouble(text, "%.0f", observation.roi.y);
    text += " ";
    AppendDouble(text, "%.0f", observation.roi.width);
    text += "x";
    AppendDouble(text, "%.0f", observation.roi.height);
    text += " (no OCR; measured thumbnail geometry only)";
    return text;
}

NavigatorObserver::NavigatorObserver(NavigatorObserverOptions options)
    : options_(options)
{
    last_ = MakeBase(NavigatorSource::Disabled, NavigatorReject::Disabled);
}

void NavigatorObserver::Disable() noexcept
{
    enabled_ = false;
    roiApplied_ = false;
    roi_ = {};
    mapping_ = {};
    last_ = MakeBase(NavigatorSource::Disabled, NavigatorReject::Disabled);
}

bool NavigatorObserver::SetRoi(
    core::Rect2 roi,
    double clientWidth,
    double clientHeight,
    core::RoiRejectReason& reason)
{
    std::optional<core::Rect2> const valid =
        core::TryValidateCanvasRoi(roi, clientWidth, clientHeight, reason);
    if (!valid.has_value())
    {
        return false;
    }

    enabled_ = true;
    roiApplied_ = true;
    roi_ = *valid;
    last_ = MakeBase(NavigatorSource::Missing, NavigatorReject::BlankOrNoIndicator);
    last_.roi = roi_;
    last_.mapping = mapping_;
    return true;
}

void NavigatorObserver::ClearMapping() noexcept
{
    mapping_ = {};
    last_.mapping = mapping_;
    last_.hasDocumentPosition = false;
    last_.hasZoomPercent = false;
    last_.documentX = 0.0;
    last_.documentY = 0.0;
    last_.zoomPercent = 0.0;
}

void NavigatorObserver::SetMapping(double documentWidth, double documentHeight) noexcept
{
    mapping_ = {};
    if (!IsFiniteValue(documentWidth) || !IsFiniteValue(documentHeight) || documentWidth <= 0.0 ||
        documentHeight <= 0.0)
    {
        last_.mapping = mapping_;
        last_.hasDocumentPosition = false;
        last_.hasZoomPercent = false;
        return;
    }

    mapping_.applied = true;
    mapping_.documentWidth = documentWidth;
    mapping_.documentHeight = documentHeight;
    mapping_.units = core::ClassifyDocumentScale(documentWidth, documentHeight);
    last_.mapping = mapping_;
}

NavigatorObservation NavigatorObserver::Observe(NavigatorView const& view)
{
    if (!enabled_)
    {
        last_ = MakeBase(NavigatorSource::Disabled, NavigatorReject::Disabled);
        last_.captureTicks = view.captureTicks;
        last_.sequence = view.sequence;
        last_.targetGeneration = view.targetGeneration;
        last_.geometryGeneration = view.geometryGeneration;
        return last_;
    }
    if (!roiApplied_)
    {
        last_ = MakeBase(NavigatorSource::UncalibratedRoi, NavigatorReject::NoRoi);
        last_.captureTicks = view.captureTicks;
        last_.sequence = view.sequence;
        last_.targetGeneration = view.targetGeneration;
        last_.geometryGeneration = view.geometryGeneration;
        return last_;
    }

    last_ = ObservePixels(view);
    return last_;
}

NavigatorObservation const& NavigatorObserver::Last() const noexcept
{
    return last_;
}

bool NavigatorObserver::Enabled() const noexcept
{
    return enabled_;
}

bool NavigatorObserver::RoiApplied() const noexcept
{
    return roiApplied_;
}

NavigatorMapping const& NavigatorObserver::Mapping() const noexcept
{
    return mapping_;
}

core::Rect2 NavigatorObserver::Roi() const noexcept
{
    return roi_;
}

NavigatorObservation NavigatorObserver::MakeBase(
    NavigatorSource source,
    NavigatorReject reject) const
{
    NavigatorObservation observation{};
    observation.source = source;
    observation.reject = reject;
    observation.roi = roi_;
    observation.mapping = mapping_;
    observation.hasParity = false;
    return observation;
}

NavigatorObservation NavigatorObserver::ObservePixels(NavigatorView const& view)
{
    NavigatorObservation observation =
        MakeBase(NavigatorSource::Missing, NavigatorReject::BlankOrNoIndicator);
    observation.captureTicks = view.captureTicks;
    observation.sequence = view.sequence;
    observation.targetGeneration = view.targetGeneration;
    observation.geometryGeneration = view.geometryGeneration;

    if (view.data == nullptr || view.width < options_.minWidth || view.height < options_.minHeight)
    {
        observation.reject = NavigatorReject::DegenerateSize;
        return observation;
    }

    int const minStride =
        view.format == NavigatorPixelFormat::Bgra32 ? view.width * 4 : view.width;
    if (view.stride < minStride)
    {
        observation.reject = NavigatorReject::DegenerateSize;
        return observation;
    }

    cv::Mat const gray = ToGray(view);
    if (gray.empty())
    {
        observation.reject = NavigatorReject::DegenerateSize;
        return observation;
    }
    if (GrayVariance(gray) < options_.minTextureVariance)
    {
        observation.reject = NavigatorReject::BlankOrNoIndicator;
        return observation;
    }

    std::vector<IndicatorCandidate> const candidates = DetectIndicators(gray, options_);
    observation.candidateCount = static_cast<int>(candidates.size());
    if (candidates.empty())
    {
        return observation;
    }

    IndicatorCandidate const& best = candidates.front();
    if (candidates.size() >= 2)
    {
        double const second = candidates[1].score;
        if (second >= options_.ambiguityRatio * best.score)
        {
            observation.source = NavigatorSource::Ambiguous;
            observation.reject = NavigatorReject::Ambiguous;
            observation.confidence = 0.0;
            return observation;
        }
    }

    double const imageW = static_cast<double>(gray.cols);
    double const imageH = static_cast<double>(gray.rows);
    double const boxW = static_cast<double>(best.box.size.width);
    double const boxH = static_cast<double>(best.box.size.height);
    if (!IsFiniteValue(best.box.center.x) || !IsFiniteValue(best.box.center.y) ||
        !IsFiniteValue(boxW) || !IsFiniteValue(boxH) || boxW <= 0.0 || boxH <= 0.0)
    {
        observation.reject = NavigatorReject::NonFinite;
        return observation;
    }

    if (best.occluded)
    {
        observation.source = NavigatorSource::Occluded;
        observation.reject = NavigatorReject::Occluded;
        observation.confidence = Clamp01(best.rectangularity * 0.25);
        return observation;
    }

    observation.source = NavigatorSource::Observed;
    observation.reject = NavigatorReject::Ok;
    observation.hasTranslation = true;
    observation.hasRelativeScale = true;
    cv::Rect const bounds = best.box.boundingRect();
    observation.centerU = Clamp01(static_cast<double>(best.box.center.x) / imageW);
    observation.centerV = Clamp01(static_cast<double>(best.box.center.y) / imageH);
    observation.sizeU = Clamp01(static_cast<double>(bounds.width) / imageW);
    observation.sizeV = Clamp01(static_cast<double>(bounds.height) / imageH);

    double const aspectGap =
        std::fabs(boxW - boxH) / std::max(std::max(boxW, boxH), 1.0);
    if (aspectGap >= options_.squareAspectLimit)
    {
        observation.hasRotation = true;
        observation.radiansClockwise = LongerEdgeRadiansClockwise(best.box);
    }

    observation.confidence = Clamp01(best.rectangularity);
    observation.hasParity = false;

    if (mapping_.applied)
    {
        observation.hasDocumentPosition = true;
        observation.documentX = observation.centerU * mapping_.documentWidth;
        observation.documentY = observation.centerV * mapping_.documentHeight;
        if (mapping_.units == core::DocumentScaleLabel::DeclaredDocumentPixels &&
            observation.sizeU > 1.0e-6)
        {
            observation.hasZoomPercent = true;
            observation.zoomPercent = 100.0 / observation.sizeU;
        }
    }

    return observation;
}

} // namespace tracing::tracking
