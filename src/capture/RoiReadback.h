#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace tracing::capture {

inline constexpr int kRoiBgraBytesPerPixel = 4;
inline constexpr int kMaxRoiAxis = 16384;
inline constexpr std::uint64_t kMaxRoiBytes = 64ull * 1024ull * 1024ull;
inline constexpr int kRoiDownsampleMaxAxis = 1024;

enum class RoiKind
{
    Canvas,
    Navigator,
};

enum class RoiClipReject
{
    Ok,
    NonPositiveSize,
    Overflow,
    EmptyAfterClip,
    BadDownsample,
    OutputEmpty,
    OversizedAxis,
    ByteLimit,
};

enum class RoiHandoffAction
{
    RejectInvalid,
    DropOldestThenEnqueue,
    Enqueue,
};

struct RoiPixelRect
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct RoiClipRequest
{
    RoiKind kind = RoiKind::Canvas;
    int sourceWidth = 0;
    int sourceHeight = 0;
    RoiPixelRect requested{};
    int downsample = 1;
};

struct RoiClipResult
{
    RoiClipReject reason = RoiClipReject::NonPositiveSize;
    RoiKind kind = RoiKind::Canvas;
    RoiPixelRect clipped{};
    int outWidth = 0;
    int outHeight = 0;
    int downsample = 1;
    int destStride = 0;
    std::uint64_t destBytes = 0;
};

struct RoiBufferMeta
{
    std::uint64_t sequence = 0;
    std::int64_t captureTicks = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    RoiKind kind = RoiKind::Canvas;
};

struct RoiHandoffResult
{
    RoiHandoffAction action = RoiHandoffAction::RejectInvalid;
    RoiBufferMeta dropped{};
    bool droppedValid = false;
};

struct RoiCpuBuffer
{
    RoiBufferMeta meta{};
    int width = 0;
    int height = 0;
    int stride = 0;
    int sourcePitch = 0;
    int downsample = 1;
    std::vector<std::uint8_t> bgra;
};

struct RoiReadbackDiagnostics
{
    bool requested = false;
    RoiKind kind = RoiKind::Canvas;
    RoiClipReject lastClip = RoiClipReject::Ok;
    RoiPixelRect clipped{};
    int outWidth = 0;
    int outHeight = 0;
    int downsample = 1;
    int srcPitch = 0;
    int destStride = 0;
    std::uint64_t copyUs = 0;
    std::uint64_t mapWaitUs = 0;
    std::size_t pending = 0;
    std::uint64_t dropped = 0;
    std::uint64_t completed = 0;
    std::uint64_t mapNotReady = 0;
    std::uint64_t recreates = 0;
    std::uint64_t lastSequence = 0;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    HRESULT lastHr = S_OK;
    bool hasCpuBuffer = false;
    bool zeroCopy = false;
};

char const* FormatRoiKind(RoiKind kind) noexcept;
char const* FormatRoiClipReject(RoiClipReject reason) noexcept;
char const* FormatRoiHandoffAction(RoiHandoffAction action) noexcept;

int DefaultRoiDownsample(int width, int height) noexcept;
RoiClipResult TryClipRoi(RoiClipRequest const& request) noexcept;
bool PackBgraRows(
    std::uint8_t const* src,
    int srcPitch,
    std::uint8_t* dst,
    int dstPitch,
    int srcWidth,
    int srcHeight,
    int downsample) noexcept;

// GPU-free bounded pending policy. kMaxPending matches the staging ring.
class RoiReadbackPolicy
{
public:
    static constexpr std::size_t kMaxPending = 2;

    RoiHandoffResult Arrive(RoiBufferMeta const& meta) noexcept;
    void NoteCompleted() noexcept;
    void DiscardAll() noexcept;
    void Reset() noexcept;

    std::size_t Pending() const noexcept;
    std::uint64_t Dropped() const noexcept;
    std::uint64_t Completed() const noexcept;
    RoiBufferMeta Oldest() const noexcept;
    RoiBufferMeta Newest() const noexcept;

private:
    RoiBufferMeta pending_[2]{};
    std::size_t count_ = 0;
    std::uint64_t dropped_ = 0;
    std::uint64_t completed_ = 0;
};

std::wstring FormatRoiReadbackReport(RoiReadbackDiagnostics const& diagnostics);

// Two-slot staging ring. Copy and Map run on the graphics/UI thread only.
// Never maps the slot that was just copied. CPU buffers are packed copies.
class RoiReadback
{
public:
    RoiReadback() = default;
    ~RoiReadback();

    RoiReadback(RoiReadback const&) = delete;
    RoiReadback& operator=(RoiReadback const&) = delete;
    RoiReadback(RoiReadback&&) = delete;
    RoiReadback& operator=(RoiReadback&&) = delete;

    void Release() noexcept;

    bool SubmitCopy(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* source,
        RoiKind kind,
        RoiPixelRect requested,
        int downsample,
        RoiBufferMeta const& meta,
        std::wstring& error);
    bool TryComplete(ID3D11DeviceContext* context, std::wstring& error);

    bool HasBuffer() const noexcept;
    RoiCpuBuffer const& LastBuffer() const noexcept;
    RoiReadbackDiagnostics Diagnostics() const noexcept;
    std::wstring FormatReport() const;

private:
    struct Slot
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        bool occupied = false;
        RoiBufferMeta meta{};
        RoiClipResult clip{};
    };

    bool EnsureStaging(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        std::wstring& error);
    void UnmapIfMapped(ID3D11DeviceContext* context) noexcept;

    Slot slots_[2]{};
    int write_ = 0;
    RoiReadbackPolicy policy_;
    RoiCpuBuffer lastBuffer_{};
    bool hasBuffer_ = false;
    bool mapped_ = false;
    int mappedSlot_ = -1;
    UINT stagingWidth_ = 0;
    UINT stagingHeight_ = 0;
    DXGI_FORMAT stagingFormat_ = DXGI_FORMAT_UNKNOWN;
    RoiReadbackDiagnostics diagnostics_{};
};

inline char const* FormatRoiKind(RoiKind kind) noexcept
{
    switch (kind)
    {
    case RoiKind::Canvas:
        return "canvas";
    case RoiKind::Navigator:
        return "navigator";
    }
    return "unknown";
}

inline char const* FormatRoiClipReject(RoiClipReject reason) noexcept
{
    switch (reason)
    {
    case RoiClipReject::Ok:
        return "ok";
    case RoiClipReject::NonPositiveSize:
        return "non-positive-size";
    case RoiClipReject::Overflow:
        return "overflow";
    case RoiClipReject::EmptyAfterClip:
        return "empty-after-clip";
    case RoiClipReject::BadDownsample:
        return "bad-downsample";
    case RoiClipReject::OutputEmpty:
        return "output-empty";
    case RoiClipReject::OversizedAxis:
        return "oversized-axis";
    case RoiClipReject::ByteLimit:
        return "byte-limit";
    }
    return "unknown";
}

inline char const* FormatRoiHandoffAction(RoiHandoffAction action) noexcept
{
    switch (action)
    {
    case RoiHandoffAction::RejectInvalid:
        return "reject-invalid";
    case RoiHandoffAction::DropOldestThenEnqueue:
        return "drop-oldest-then-enqueue";
    case RoiHandoffAction::Enqueue:
        return "enqueue";
    }
    return "unknown";
}

inline int DefaultRoiDownsample(int width, int height) noexcept
{
    int const m = width > height ? width : height;
    if (m <= 0 || m <= kRoiDownsampleMaxAxis)
    {
        return 1;
    }
    int d = m / kRoiDownsampleMaxAxis;
    if (m % kRoiDownsampleMaxAxis != 0)
    {
        ++d;
    }
    return d < 1 ? 1 : d;
}

inline RoiClipResult TryClipRoi(RoiClipRequest const& request) noexcept
{
    RoiClipResult result{};
    result.kind = request.kind;
    result.downsample = request.downsample;
    result.reason = RoiClipReject::NonPositiveSize;

    if (request.sourceWidth <= 0 || request.sourceHeight <= 0 || request.requested.w <= 0 ||
        request.requested.h <= 0)
    {
        return result;
    }
    if (request.downsample < 1)
    {
        result.reason = RoiClipReject::BadDownsample;
        return result;
    }

    std::int64_t const sourceW = request.sourceWidth;
    std::int64_t const sourceH = request.sourceHeight;
    std::int64_t left = request.requested.x;
    std::int64_t top = request.requested.y;
    std::int64_t right = left + static_cast<std::int64_t>(request.requested.w);
    std::int64_t bottom = top + static_cast<std::int64_t>(request.requested.h);

    if (left < 0)
    {
        left = 0;
    }
    if (top < 0)
    {
        top = 0;
    }
    if (right > sourceW)
    {
        right = sourceW;
    }
    if (bottom > sourceH)
    {
        bottom = sourceH;
    }
    if (right <= left || bottom <= top)
    {
        result.reason = RoiClipReject::EmptyAfterClip;
        return result;
    }

    std::int64_t const clippedW = right - left;
    std::int64_t const clippedH = bottom - top;
    if (clippedW > kMaxRoiAxis || clippedH > kMaxRoiAxis)
    {
        result.reason = RoiClipReject::OversizedAxis;
        return result;
    }

    std::int64_t const outW = clippedW / request.downsample;
    std::int64_t const outH = clippedH / request.downsample;
    if (outW <= 0 || outH <= 0)
    {
        result.reason = RoiClipReject::OutputEmpty;
        return result;
    }
    if (outW > kMaxRoiAxis || outH > kMaxRoiAxis)
    {
        result.reason = RoiClipReject::OversizedAxis;
        return result;
    }

    if (outW > (INT64_MAX / kRoiBgraBytesPerPixel))
    {
        result.reason = RoiClipReject::Overflow;
        return result;
    }
    std::int64_t const stride = outW * kRoiBgraBytesPerPixel;
    if (stride > static_cast<std::int64_t>(INT_MAX))
    {
        result.reason = RoiClipReject::Overflow;
        return result;
    }
    if (outH > 0 && stride > (INT64_MAX / outH))
    {
        result.reason = RoiClipReject::Overflow;
        return result;
    }
    std::uint64_t const bytes = static_cast<std::uint64_t>(outH) * static_cast<std::uint64_t>(stride);
    if (bytes > kMaxRoiBytes)
    {
        result.reason = RoiClipReject::ByteLimit;
        return result;
    }

    result.reason = RoiClipReject::Ok;
    result.clipped.x = static_cast<int>(left);
    result.clipped.y = static_cast<int>(top);
    result.clipped.w = static_cast<int>(clippedW);
    result.clipped.h = static_cast<int>(clippedH);
    result.outWidth = static_cast<int>(outW);
    result.outHeight = static_cast<int>(outH);
    result.destStride = static_cast<int>(stride);
    result.destBytes = bytes;
    return result;
}

inline bool PackBgraRows(
    std::uint8_t const* src,
    int srcPitch,
    std::uint8_t* dst,
    int dstPitch,
    int srcWidth,
    int srcHeight,
    int downsample) noexcept
{
    if (src == nullptr || dst == nullptr || srcPitch <= 0 || dstPitch <= 0 || srcWidth <= 0 ||
        srcHeight <= 0 || downsample < 1)
    {
        return false;
    }

    int const outW = srcWidth / downsample;
    int const outH = srcHeight / downsample;
    if (outW <= 0 || outH <= 0)
    {
        return false;
    }
    if (dstPitch < outW * kRoiBgraBytesPerPixel)
    {
        return false;
    }
    if (srcPitch < srcWidth * kRoiBgraBytesPerPixel)
    {
        return false;
    }

    for (int outY = 0; outY < outH; ++outY)
    {
        int const srcY = outY * downsample;
        std::uint8_t const* const srcRow = src + static_cast<std::ptrdiff_t>(srcY) * srcPitch;
        std::uint8_t* const dstRow = dst + static_cast<std::ptrdiff_t>(outY) * dstPitch;
        for (int outX = 0; outX < outW; ++outX)
        {
            int const srcX = outX * downsample;
            std::uint8_t const* const srcPx = srcRow + static_cast<std::ptrdiff_t>(srcX) * kRoiBgraBytesPerPixel;
            std::uint8_t* const dstPx = dstRow + static_cast<std::ptrdiff_t>(outX) * kRoiBgraBytesPerPixel;
            dstPx[0] = srcPx[0];
            dstPx[1] = srcPx[1];
            dstPx[2] = srcPx[2];
            dstPx[3] = srcPx[3];
        }
    }
    return true;
}

inline RoiHandoffResult RoiReadbackPolicy::Arrive(RoiBufferMeta const& meta) noexcept
{
    RoiHandoffResult result{};
    if (count_ >= kMaxPending)
    {
        result.action = RoiHandoffAction::DropOldestThenEnqueue;
        result.dropped = pending_[0];
        result.droppedValid = true;
        pending_[0] = pending_[1];
        pending_[1] = meta;
        ++dropped_;
        count_ = kMaxPending;
        return result;
    }

    pending_[count_] = meta;
    ++count_;
    result.action = RoiHandoffAction::Enqueue;
    return result;
}

inline void RoiReadbackPolicy::NoteCompleted() noexcept
{
    if (count_ == 0)
    {
        return;
    }
    pending_[0] = pending_[1];
    pending_[1] = {};
    --count_;
    ++completed_;
}

inline void RoiReadbackPolicy::DiscardAll() noexcept
{
    dropped_ += count_;
    count_ = 0;
    pending_[0] = {};
    pending_[1] = {};
}

inline void RoiReadbackPolicy::Reset() noexcept
{
    count_ = 0;
    pending_[0] = {};
    pending_[1] = {};
}

inline std::size_t RoiReadbackPolicy::Pending() const noexcept
{
    return count_;
}

inline std::uint64_t RoiReadbackPolicy::Dropped() const noexcept
{
    return dropped_;
}

inline std::uint64_t RoiReadbackPolicy::Completed() const noexcept
{
    return completed_;
}

inline RoiBufferMeta RoiReadbackPolicy::Oldest() const noexcept
{
    if (count_ == 0)
    {
        return {};
    }
    return pending_[0];
}

inline RoiBufferMeta RoiReadbackPolicy::Newest() const noexcept
{
    if (count_ == 0)
    {
        return {};
    }
    return pending_[count_ - 1];
}

inline std::wstring WidenAsciiLabel(char const* text)
{
    if (text == nullptr)
    {
        return {};
    }
    return std::wstring(text, text + std::strlen(text));
}

inline std::wstring FormatRoiReadbackReport(RoiReadbackDiagnostics const& diagnostics)
{
    return L"roi kind=" + WidenAsciiLabel(FormatRoiKind(diagnostics.kind)) + L" clip=" +
           WidenAsciiLabel(FormatRoiClipReject(diagnostics.lastClip)) + L" clipped=" +
           std::to_wstring(diagnostics.clipped.x) + L"," + std::to_wstring(diagnostics.clipped.y) +
           L" " + std::to_wstring(diagnostics.clipped.w) + L"x" +
           std::to_wstring(diagnostics.clipped.h) + L" out=" + std::to_wstring(diagnostics.outWidth) +
           L"x" + std::to_wstring(diagnostics.outHeight) + L" ds=" +
           std::to_wstring(diagnostics.downsample) + L" copyUs=" +
           std::to_wstring(diagnostics.copyUs) + L" mapWaitUs=" +
           std::to_wstring(diagnostics.mapWaitUs) + L" pending=" +
           std::to_wstring(diagnostics.pending) + L" dropped=" +
           std::to_wstring(diagnostics.dropped) + L" mapNotReady=" +
           std::to_wstring(diagnostics.mapNotReady) + L" recreates=" +
           std::to_wstring(diagnostics.recreates) + L" pitch=" +
           std::to_wstring(diagnostics.srcPitch) + L" stride=" +
           std::to_wstring(diagnostics.destStride) + L" seq=" +
           std::to_wstring(diagnostics.lastSequence) + L" targetGen=" +
           std::to_wstring(diagnostics.targetGeneration) + L" geomGen=" +
           std::to_wstring(diagnostics.geometryGeneration) + L" hasCpu=" +
           (diagnostics.hasCpuBuffer ? L"yes" : L"no") + L" zeroCopy=no";
}

} // namespace tracing::capture
