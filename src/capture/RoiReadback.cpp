#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "capture/RoiReadback.h"

#include <dxgi.h>

#include <cstdio>
#include <new>
#include <utility>

namespace tracing::capture {
namespace {

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

std::uint64_t ElapsedMicros(LARGE_INTEGER start, LARGE_INTEGER end, LARGE_INTEGER freq) noexcept
{
    if (freq.QuadPart <= 0 || end.QuadPart < start.QuadPart)
    {
        return 0;
    }
    return static_cast<std::uint64_t>(
        ((end.QuadPart - start.QuadPart) * 1'000'000) / freq.QuadPart);
}

} // namespace

RoiReadback::~RoiReadback()
{
    Release();
}

void RoiReadback::UnmapIfMapped(ID3D11DeviceContext* context) noexcept
{
    if (!mapped_ || mappedSlot_ < 0 || mappedSlot_ > 1 || context == nullptr)
    {
        mapped_ = false;
        mappedSlot_ = -1;
        return;
    }
    if (slots_[mappedSlot_].staging)
    {
        context->Unmap(slots_[mappedSlot_].staging.Get(), 0);
    }
    mapped_ = false;
    mappedSlot_ = -1;
}

void RoiReadback::Release() noexcept
{
    mapped_ = false;
    mappedSlot_ = -1;
    slots_[0].staging.Reset();
    slots_[1].staging.Reset();
    slots_[0].occupied = false;
    slots_[1].occupied = false;
    write_ = 0;
    policy_.Reset();
    lastBuffer_ = {};
    hasBuffer_ = false;
    stagingWidth_ = 0;
    stagingHeight_ = 0;
    stagingFormat_ = DXGI_FORMAT_UNKNOWN;
    diagnostics_ = {};
}

bool RoiReadback::EnsureStaging(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    std::wstring& error)
{
    error.clear();
    if (device == nullptr || width == 0 || height == 0)
    {
        error = L"ROI staging needs a ready D3D11 device and positive size.";
        diagnostics_.lastHr = E_INVALIDARG;
        return false;
    }
    if (slots_[0].staging && slots_[1].staging && stagingWidth_ == width &&
        stagingHeight_ == height && stagingFormat_ == format)
    {
        return true;
    }

    bool const replacing = slots_[0].staging || slots_[1].staging;
    UnmapIfMapped(context);
    slots_[0].staging.Reset();
    slots_[1].staging.Reset();
    slots_[0].occupied = false;
    slots_[1].occupied = false;
    write_ = 0;
    policy_.DiscardAll();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &slots_[0].staging);
    if (FAILED(hr) || !slots_[0].staging)
    {
        diagnostics_.lastHr = hr;
        error = L"CreateTexture2D ROI staging[0] failed HRESULT=" + FormatHresult(hr);
        return false;
    }
    hr = device->CreateTexture2D(&desc, nullptr, &slots_[1].staging);
    if (FAILED(hr) || !slots_[1].staging)
    {
        slots_[0].staging.Reset();
        diagnostics_.lastHr = hr;
        error = L"CreateTexture2D ROI staging[1] failed HRESULT=" + FormatHresult(hr);
        return false;
    }

    stagingWidth_ = width;
    stagingHeight_ = height;
    stagingFormat_ = format;
    if (replacing)
    {
        ++diagnostics_.recreates;
    }
    diagnostics_.lastHr = S_OK;
    diagnostics_.pending = policy_.Pending();
    diagnostics_.dropped = policy_.Dropped();
    return true;
}

bool RoiReadback::SubmitCopy(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* source,
    RoiKind kind,
    RoiPixelRect requested,
    int downsample,
    RoiBufferMeta const& meta,
    std::wstring& error)
{
    error.clear();
    diagnostics_.requested = true;
    diagnostics_.kind = kind;
    diagnostics_.zeroCopy = false;

    if (device == nullptr || context == nullptr || source == nullptr)
    {
        diagnostics_.lastClip = RoiClipReject::NonPositiveSize;
        diagnostics_.lastHr = E_INVALIDARG;
        error = L"ROI SubmitCopy needs device, context, and source texture.";
        return false;
    }

    D3D11_TEXTURE2D_DESC sourceDesc{};
    source->GetDesc(&sourceDesc);

    RoiClipRequest request{};
    request.kind = kind;
    request.sourceWidth = static_cast<int>(sourceDesc.Width);
    request.sourceHeight = static_cast<int>(sourceDesc.Height);
    request.requested = requested;
    request.downsample = downsample;
    RoiClipResult const clip = TryClipRoi(request);
    diagnostics_.lastClip = clip.reason;
    diagnostics_.clipped = clip.clipped;
    diagnostics_.outWidth = clip.outWidth;
    diagnostics_.outHeight = clip.outHeight;
    diagnostics_.downsample = clip.downsample;
    diagnostics_.destStride = clip.destStride;
    if (clip.reason != RoiClipReject::Ok)
    {
        diagnostics_.lastHr = E_INVALIDARG;
        error = L"ROI clip rejected: " + WidenAsciiLabel(FormatRoiClipReject(clip.reason));
        return false;
    }

    if (!EnsureStaging(
            device,
            context,
            static_cast<UINT>(clip.clipped.w),
            static_cast<UINT>(clip.clipped.h),
            sourceDesc.Format,
            error))
    {
        return false;
    }

    UnmapIfMapped(context);
    slots_[write_].occupied = false;
    policy_.Arrive(meta);

    D3D11_BOX box{};
    box.left = static_cast<UINT>(clip.clipped.x);
    box.top = static_cast<UINT>(clip.clipped.y);
    box.front = 0;
    box.right = static_cast<UINT>(clip.clipped.x + clip.clipped.w);
    box.bottom = static_cast<UINT>(clip.clipped.y + clip.clipped.h);
    box.back = 1;

    LARGE_INTEGER freq{};
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    context->CopySubresourceRegion(
        slots_[write_].staging.Get(),
        0,
        0,
        0,
        0,
        source,
        0,
        &box);
    QueryPerformanceCounter(&end);

    slots_[write_].occupied = true;
    slots_[write_].meta = meta;
    slots_[write_].clip = clip;
    write_ = 1 - write_;

    diagnostics_.copyUs = ElapsedMicros(start, end, freq);
    diagnostics_.lastHr = S_OK;
    diagnostics_.pending = policy_.Pending();
    diagnostics_.dropped = policy_.Dropped();
    diagnostics_.completed = policy_.Completed();
    diagnostics_.lastSequence = meta.sequence;
    diagnostics_.targetGeneration = meta.targetGeneration;
    diagnostics_.geometryGeneration = meta.geometryGeneration;
    return true;
}

bool RoiReadback::TryComplete(ID3D11DeviceContext* context, std::wstring& error)
{
    error.clear();
    if (context == nullptr)
    {
        diagnostics_.lastHr = E_INVALIDARG;
        error = L"ROI TryComplete needs the immediate context.";
        return false;
    }

    int const read = write_;
    if (!slots_[read].occupied || !slots_[read].staging)
    {
        diagnostics_.pending = policy_.Pending();
        return false;
    }

    UnmapIfMapped(context);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    LARGE_INTEGER freq{};
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    HRESULT const hr =
        context->Map(slots_[read].staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    QueryPerformanceCounter(&end);
    diagnostics_.mapWaitUs = ElapsedMicros(start, end, freq);
    diagnostics_.lastHr = hr;

    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
    {
        ++diagnostics_.mapNotReady;
        diagnostics_.pending = policy_.Pending();
        return false;
    }
    if (FAILED(hr) || mapped.pData == nullptr)
    {
        error = L"Map ROI staging failed HRESULT=" + FormatHresult(hr);
        return false;
    }

    mapped_ = true;
    mappedSlot_ = read;

    RoiClipResult const clip = slots_[read].clip;
    RoiCpuBuffer buffer{};
    buffer.meta = slots_[read].meta;
    buffer.width = clip.outWidth;
    buffer.height = clip.outHeight;
    buffer.stride = clip.destStride;
    buffer.sourcePitch = static_cast<int>(mapped.RowPitch);
    buffer.downsample = clip.downsample;
    try
    {
        buffer.bgra.resize(static_cast<std::size_t>(clip.destBytes));
    }
    catch (std::bad_alloc const&)
    {
        UnmapIfMapped(context);
        diagnostics_.lastHr = E_OUTOFMEMORY;
        error = L"ROI CPU buffer allocation failed.";
        return false;
    }

    bool const packed = PackBgraRows(
        static_cast<std::uint8_t const*>(mapped.pData),
        buffer.sourcePitch,
        buffer.bgra.data(),
        buffer.stride,
        clip.clipped.w,
        clip.clipped.h,
        clip.downsample);
    UnmapIfMapped(context);
    if (!packed)
    {
        diagnostics_.lastHr = E_FAIL;
        error = L"ROI row pack failed (pitch/downsample).";
        return false;
    }

    lastBuffer_ = std::move(buffer);
    hasBuffer_ = true;
    slots_[read].occupied = false;
    policy_.NoteCompleted();

    diagnostics_.srcPitch = lastBuffer_.sourcePitch;
    diagnostics_.destStride = lastBuffer_.stride;
    diagnostics_.hasCpuBuffer = true;
    diagnostics_.pending = policy_.Pending();
    diagnostics_.dropped = policy_.Dropped();
    diagnostics_.completed = policy_.Completed();
    diagnostics_.lastSequence = lastBuffer_.meta.sequence;
    diagnostics_.targetGeneration = lastBuffer_.meta.targetGeneration;
    diagnostics_.geometryGeneration = lastBuffer_.meta.geometryGeneration;
    diagnostics_.lastHr = S_OK;
    return true;
}

bool RoiReadback::HasBuffer() const noexcept
{
    return hasBuffer_;
}

RoiCpuBuffer const& RoiReadback::LastBuffer() const noexcept
{
    return lastBuffer_;
}

RoiReadbackDiagnostics RoiReadback::Diagnostics() const noexcept
{
    RoiReadbackDiagnostics snapshot = diagnostics_;
    snapshot.pending = policy_.Pending();
    snapshot.dropped = policy_.Dropped();
    snapshot.completed = policy_.Completed();
    snapshot.hasCpuBuffer = hasBuffer_;
    snapshot.zeroCopy = false;
    return snapshot;
}

std::wstring RoiReadback::FormatReport() const
{
    return FormatRoiReadbackReport(Diagnostics());
}

} // namespace tracing::capture
