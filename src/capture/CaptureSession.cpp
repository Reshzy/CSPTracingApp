#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "capture/CaptureSession.h"
#include "capture/RoiReadback.h"

#include <Unknwn.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <dxgi.h>
#include <wrl/client.h>

#include <cstdio>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace tracing::capture {
namespace {

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgd = winrt::Windows::Graphics::DirectX;
namespace wgd3d = winrt::Windows::Graphics::DirectX::Direct3D11;

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

void CloseFrameQuiet(wgc::Direct3D11CaptureFrame& frame) noexcept
{
    if (!frame)
    {
        return;
    }
    try
    {
        frame.Close();
    }
    catch (...)
    {
    }
    frame = nullptr;
}

winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice WrapD3dDevice(ID3D11Device* device)
{
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)));
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put()));
    return inspectable.as<wgd3d::IDirect3DDevice>();
}

wgc::GraphicsCaptureItem CreateItemForWindow(HWND hwnd)
{
    auto const factory = winrt::get_activation_factory<wgc::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    wgc::GraphicsCaptureItem item{nullptr};
    winrt::check_hresult(factory->CreateForWindow(
        hwnd,
        winrt::guid_of<wgc::GraphicsCaptureItem>(),
        winrt::put_abi(item)));
    return item;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> TextureFromSurface(wgd3d::IDirect3DSurface const& surface)
{
    auto const access = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&texture)));
    return texture;
}

RoiCpuSnapshot PackRoiSnapshot(RoiCpuBuffer const& packed)
{
    RoiCpuSnapshot snapshot{};
    snapshot.sequence = packed.meta.sequence;
    snapshot.captureTicks = packed.meta.captureTicks;
    snapshot.targetGeneration = packed.meta.targetGeneration;
    snapshot.geometryGeneration = packed.meta.geometryGeneration;
    snapshot.width = packed.width;
    snapshot.height = packed.height;
    snapshot.stride = packed.stride;
    snapshot.downsample = packed.downsample;
    snapshot.bgra = packed.bgra;
    snapshot.valid = packed.width > 0 && packed.height > 0 && !packed.bgra.empty();
    return snapshot;
}

bool IsDxgiDeviceLost(HRESULT hr) noexcept
{
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
           hr == DXGI_ERROR_DEVICE_HUNG;
}

bool NoteDeviceLostAfterGpu(
    graphics::DeviceResources* gpu,
    HRESULT& copyHr,
    std::wstring& error)
{
    HRESULT removed = S_OK;
    bool const removedByDevice = gpu != nullptr && gpu->CheckDeviceRemoved(removed);
    if (!removedByDevice && !IsDxgiDeviceLost(copyHr))
    {
        return false;
    }
    if (!FAILED(copyHr))
    {
        copyHr = removed != S_OK ? removed : DXGI_ERROR_DEVICE_REMOVED;
    }
    error = L"WGC owned copy aborted: device removed HRESULT=" + FormatHresult(copyHr);
    return true;
}

} // namespace

struct CaptureSession::Impl
{
    mutable std::mutex mutex;
    CaptureSessionPolicy policy;
    FramePacket lastPacket{};
    HRESULT lastHr = S_OK;
    bool supported = false;
    bool supportChecked = false;
    bool itemClosed = false;
    bool hasOwnedFrame = false;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    HWND notifyWindow = nullptr;
    UINT notifyMessage = 0;
    graphics::DeviceResources* device = nullptr;

    wgd3d::IDirect3DDevice winrtDevice{nullptr};
    wgc::GraphicsCaptureItem item{nullptr};
    wgc::Direct3D11CaptureFramePool framePool{nullptr};
    wgc::GraphicsCaptureSession session{nullptr};
    winrt::event_token arrivedToken{};
    winrt::event_token closedToken{};

    struct PendingItem
    {
        wgc::Direct3D11CaptureFrame frame{nullptr};
        FramePacket packet{};
    };
    std::deque<PendingItem> pending;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> ownedTexture;
    UINT ownedWidth = 0;
    UINT ownedHeight = 0;
    DXGI_FORMAT ownedFormat = DXGI_FORMAT_UNKNOWN;
    int poolWidth = 0;
    int poolHeight = 0;
    std::uint64_t recreateCount = 0;
    HRESULT lastRecreateHr = S_OK;
    RoiReadback roiReadback;
    RoiReadback navigatorReadback;
    std::wstring roiReport{L"roi (none)"};
    std::wstring roiSrc{L"full-content-fallback"};
    std::wstring navigatorRoiReport{L"navRoi (none)"};
    std::wstring navigatorRoiSrc{L"none"};
    CanvasRoiRequest canvasRoi{};
    NavigatorRoiRequest navigatorRoi{};
    RoiCpuSnapshot lastRoiBuffer{};
    RoiCpuSnapshot lastNavigatorRoiBuffer{};
    bool hasRoiBuffer = false;
    bool hasNavigatorRoiBuffer = false;

    void OnFrameArrived(wgc::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&);
    void OnClosed(wgc::GraphicsCaptureItem const&, winrt::Windows::Foundation::IInspectable const&);
    void RevokeEvents() noexcept;
    void CloseSessionAndPool() noexcept;
    void TeardownResources() noexcept;
    bool CopyOwned(PendingItem& pendingItem, HRESULT& copyHr, std::wstring& error);
    bool RecreatePoolIfContentSizeChanged(int contentWidth, int contentHeight, HRESULT& recreateHr, std::wstring& error);
};

CaptureSession::CaptureSession() : impl_(std::make_unique<Impl>()) {}

CaptureSession::~CaptureSession()
{
    Stop();
}

void CaptureSession::Impl::RevokeEvents() noexcept
{
    try
    {
        if (framePool && arrivedToken)
        {
            framePool.FrameArrived(arrivedToken);
        }
    }
    catch (...)
    {
    }
    arrivedToken = {};
    try
    {
        if (item && closedToken)
        {
            item.Closed(closedToken);
        }
    }
    catch (...)
    {
    }
    closedToken = {};
}

void CaptureSession::Impl::CloseSessionAndPool() noexcept
{
    try
    {
        if (session)
        {
            session.Close();
        }
    }
    catch (...)
    {
    }
    session = nullptr;
    try
    {
        if (framePool)
        {
            framePool.Close();
        }
    }
    catch (...)
    {
    }
    framePool = nullptr;
    item = nullptr;
    try
    {
        if (winrtDevice)
        {
            winrtDevice.Close();
        }
    }
    catch (...)
    {
    }
    winrtDevice = nullptr;
}

void CaptureSession::Impl::TeardownResources() noexcept
{
    RevokeEvents();
    CloseSessionAndPool();

    std::deque<PendingItem> local;
    {
        std::lock_guard<std::mutex> const lock(mutex);
        local.swap(pending);
        device = nullptr;
        hasOwnedFrame = false;
        hasRoiBuffer = false;
        hasNavigatorRoiBuffer = false;
        lastRoiBuffer = {};
        lastNavigatorRoiBuffer = {};
    }
    for (auto& queued : local)
    {
        CloseFrameQuiet(queued.frame);
    }

    roiReadback.Release();
    navigatorReadback.Release();
    roiReport = L"roi (none)";
    roiSrc = L"full-content-fallback";
    navigatorRoiReport = L"navRoi (none)";
    navigatorRoiSrc = L"none";
    canvasRoi = {};
    navigatorRoi = {};

    ownedTexture.Reset();
    ownedWidth = 0;
    ownedHeight = 0;
    ownedFormat = DXGI_FORMAT_UNKNOWN;
    poolWidth = 0;
    poolHeight = 0;
}

void CaptureSession::Impl::OnClosed(
    wgc::GraphicsCaptureItem const&,
    winrt::Windows::Foundation::IInspectable const&)
{
    HWND notify = nullptr;
    UINT message = 0;
    {
        std::lock_guard<std::mutex> const lock(mutex);
        itemClosed = true;
        policy.Apply(CaptureSessionEvent::ItemClosed);
        notify = notifyWindow;
        message = notifyMessage;
    }
    if (notify != nullptr && message != 0)
    {
        PostMessageW(notify, message, 1, 0);
    }
}

void CaptureSession::Impl::OnFrameArrived(
    wgc::Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&)
{
    wgc::Direct3D11CaptureFrame frame{nullptr};
    try
    {
        frame = sender.TryGetNextFrame();
    }
    catch (winrt::hresult_error const& error)
    {
        std::lock_guard<std::mutex> const lock(mutex);
        lastHr = error.code();
        return;
    }
    catch (...)
    {
        return;
    }
    if (!frame)
    {
        return;
    }

    winrt::Windows::Graphics::SizeInt32 size{};
    winrt::Windows::Foundation::TimeSpan timestamp{};
    try
    {
        size = frame.ContentSize();
        timestamp = frame.SystemRelativeTime();
    }
    catch (...)
    {
        CloseFrameQuiet(frame);
        return;
    }

    CaptureFrameArrival arrival{};
    HWND notify = nullptr;
    UINT message = 0;
    CaptureHandoffResult result{};
    PendingItem dropped{};
    bool post = false;
    bool closeNow = true;

    {
        std::lock_guard<std::mutex> const lock(mutex);
        arrival.targetGeneration = targetGeneration;
        arrival.contentWidth = size.Width;
        arrival.contentHeight = size.Height;
        result = policy.Arrive(arrival);
        notify = notifyWindow;
        message = notifyMessage;

        if (result.action == CaptureHandoffAction::DropOldestThenEnqueue && !pending.empty())
        {
            dropped = std::move(pending.front());
            pending.pop_front();
        }

        if (result.action == CaptureHandoffAction::Enqueue ||
            result.action == CaptureHandoffAction::DropOldestThenEnqueue)
        {
            PendingItem queued;
            queued.frame = frame;
            queued.packet.sequence = result.sequence;
            queued.packet.captureTicks = timestamp.count();
            queued.packet.contentWidth = size.Width;
            queued.packet.contentHeight = size.Height;
            queued.packet.targetGeneration = arrival.targetGeneration;
            queued.packet.geometryGeneration = geometryGeneration;
            queued.packet.stale = false;
            pending.push_back(std::move(queued));
            closeNow = false;
            post = true;
        }
        else if (result.action == CaptureHandoffAction::StaleZeroSize)
        {
            lastPacket.sequence = policy.LastSequence();
            lastPacket.captureTicks = timestamp.count();
            lastPacket.contentWidth = size.Width;
            lastPacket.contentHeight = size.Height;
            lastPacket.targetGeneration = arrival.targetGeneration;
            lastPacket.geometryGeneration = geometryGeneration;
            lastPacket.stale = true;
            post = true;
        }
    }

    CloseFrameQuiet(dropped.frame);
    if (closeNow)
    {
        CloseFrameQuiet(frame);
    }
    if (post && notify != nullptr && message != 0)
    {
        PostMessageW(notify, message, 0, 0);
    }
}

bool CaptureSession::Impl::CopyOwned(PendingItem& pendingItem, HRESULT& copyHr, std::wstring& error)
{
    error.clear();
    copyHr = S_OK;
    graphics::DeviceResources* gpu = nullptr;
    {
        std::lock_guard<std::mutex> const lock(mutex);
        gpu = device;
    }
    if (!pendingItem.frame || gpu == nullptr || !gpu->IsReady())
    {
        CloseFrameQuiet(pendingItem.frame);
        error = L"Cannot copy WGC frame: device not ready.";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> source;
    try
    {
        source = TextureFromSurface(pendingItem.frame.Surface());
    }
    catch (winrt::hresult_error const& hrError)
    {
        copyHr = hrError.code();
        CloseFrameQuiet(pendingItem.frame);
        error = L"GetInterface ID3D11Texture2D failed HRESULT=" + FormatHresult(copyHr);
        return false;
    }
    catch (...)
    {
        CloseFrameQuiet(pendingItem.frame);
        error = L"GetInterface ID3D11Texture2D failed.";
        return false;
    }

    D3D11_TEXTURE2D_DESC sourceDesc{};
    source->GetDesc(&sourceDesc);

    UINT copyWidth = sourceDesc.Width;
    UINT copyHeight = sourceDesc.Height;
    if (pendingItem.packet.contentWidth > 0 && pendingItem.packet.contentHeight > 0)
    {
        copyWidth = static_cast<UINT>(pendingItem.packet.contentWidth);
        copyHeight = static_cast<UINT>(pendingItem.packet.contentHeight);
        if (copyWidth > sourceDesc.Width)
        {
            copyWidth = sourceDesc.Width;
        }
        if (copyHeight > sourceDesc.Height)
        {
            copyHeight = sourceDesc.Height;
        }
    }
    if (copyWidth == 0 || copyHeight == 0)
    {
        CloseFrameQuiet(pendingItem.frame);
        error = L"Cannot copy WGC frame: content size is zero.";
        return false;
    }

    D3D11_TEXTURE2D_DESC ownedDesc{};
    ownedDesc.Width = copyWidth;
    ownedDesc.Height = copyHeight;
    ownedDesc.MipLevels = 1;
    ownedDesc.ArraySize = 1;
    ownedDesc.Format = sourceDesc.Format;
    ownedDesc.SampleDesc.Count = 1;
    ownedDesc.SampleDesc.Quality = 0;
    ownedDesc.Usage = D3D11_USAGE_DEFAULT;
    ownedDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ownedDesc.CPUAccessFlags = 0;
    ownedDesc.MiscFlags = 0;

    if (!ownedTexture || ownedWidth != copyWidth || ownedHeight != copyHeight ||
        ownedFormat != sourceDesc.Format)
    {
        ownedTexture.Reset();
        copyHr = gpu->Device()->CreateTexture2D(&ownedDesc, nullptr, &ownedTexture);
        if (FAILED(copyHr) || !ownedTexture)
        {
            CloseFrameQuiet(pendingItem.frame);
            if (NoteDeviceLostAfterGpu(gpu, copyHr, error))
            {
                return false;
            }
            error = L"CreateTexture2D owned capture copy failed HRESULT=" + FormatHresult(copyHr);
            return false;
        }
        ownedWidth = copyWidth;
        ownedHeight = copyHeight;
        ownedFormat = sourceDesc.Format;
    }

    D3D11_BOX box{};
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = copyWidth;
    box.bottom = copyHeight;
    box.back = 1;
    gpu->ImmediateContext()->CopySubresourceRegion(
        ownedTexture.Get(),
        0,
        0,
        0,
        0,
        source.Get(),
        0,
        &box);
    CloseFrameQuiet(pendingItem.frame);
    if (NoteDeviceLostAfterGpu(gpu, copyHr, error))
    {
        ownedTexture.Reset();
        ownedWidth = 0;
        ownedHeight = 0;
        ownedFormat = DXGI_FORMAT_UNKNOWN;
        return false;
    }
    return true;
}

bool CaptureSession::Impl::RecreatePoolIfContentSizeChanged(
    int contentWidth,
    int contentHeight,
    HRESULT& recreateHr,
    std::wstring& error)
{
    error.clear();
    recreateHr = S_OK;
    if (contentWidth <= 0 || contentHeight <= 0)
    {
        return true;
    }
    if (!framePool || !winrtDevice)
    {
        return true;
    }
    if (contentWidth == poolWidth && contentHeight == poolHeight)
    {
        return true;
    }

    winrt::Windows::Graphics::SizeInt32 const newSize{contentWidth, contentHeight};
    try
    {
        framePool.Recreate(
            winrtDevice,
            wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            newSize);
        poolWidth = contentWidth;
        poolHeight = contentHeight;
        ++recreateCount;
        return true;
    }
    catch (winrt::hresult_error const& hrError)
    {
        recreateHr = hrError.code();
        error = L"Frame pool Recreate failed HRESULT=" + FormatHresult(recreateHr);
        return false;
    }
    catch (...)
    {
        recreateHr = E_FAIL;
        error = L"Frame pool Recreate failed with an unknown exception.";
        return false;
    }
}

bool CaptureSession::Start(
    HWND target,
    std::uint64_t targetGeneration,
    std::uint64_t geometryGeneration,
    graphics::DeviceResources& device,
    HWND notifyWindow,
    UINT notifyMessage,
    std::wstring& error)
{
    error.clear();
    Stop();

    if (target == nullptr || !IsWindow(target))
    {
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        error = L"WGC Start requires a live target HWND.";
        return false;
    }
    if (!device.IsReady() || device.Device() == nullptr)
    {
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        error = L"WGC Start requires a ready BGRA D3D11 device.";
        return false;
    }
    if (notifyWindow == nullptr)
    {
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        error = L"WGC Start requires a notify HWND for owned-frame handoff.";
        return false;
    }

    try
    {
        impl_->supportChecked = true;
        impl_->supported = wgc::GraphicsCaptureSession::IsSupported();
        if (!impl_->supported)
        {
            impl_->policy.Apply(CaptureSessionEvent::SupportMissing);
            error = L"Windows Graphics Capture is not supported on this system.";
            return false;
        }
        impl_->policy.Apply(CaptureSessionEvent::SupportPresent);
    }
    catch (winrt::hresult_error const& hrError)
    {
        impl_->lastHr = hrError.code();
        impl_->supportChecked = true;
        if (static_cast<HRESULT>(hrError.code()) == RPC_E_WRONG_THREAD)
        {
            impl_->supported = true;
            impl_->policy.Apply(CaptureSessionEvent::SupportPresent);
        }
        else
        {
            impl_->supported = false;
            impl_->policy.Apply(CaptureSessionEvent::SupportMissing);
            error = L"GraphicsCaptureSession::IsSupported failed HRESULT=" +
                    FormatHresult(impl_->lastHr);
            return false;
        }
    }

    try
    {
        impl_->winrtDevice = WrapD3dDevice(device.Device());
        impl_->item = CreateItemForWindow(target);
        auto const contentSize = impl_->item.Size();
        if (contentSize.Width <= 0 || contentSize.Height <= 0)
        {
            impl_->policy.Apply(CaptureSessionEvent::StartFailed);
            error = L"GraphicsCaptureItem size is empty; cannot create frame pool.";
            impl_->item = nullptr;
            impl_->winrtDevice = nullptr;
            return false;
        }

        impl_->framePool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            impl_->winrtDevice,
            wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            contentSize);
        impl_->poolWidth = contentSize.Width;
        impl_->poolHeight = contentSize.Height;
        impl_->recreateCount = 0;
        impl_->lastRecreateHr = S_OK;
        impl_->session = impl_->framePool.CreateCaptureSession(impl_->item);
        try
        {
            impl_->session.IsCursorCaptureEnabled(false);
        }
        catch (...)
        {
        }

        {
            std::lock_guard<std::mutex> const lock(impl_->mutex);
            impl_->device = &device;
            impl_->notifyWindow = notifyWindow;
            impl_->notifyMessage = notifyMessage;
            impl_->targetGeneration = targetGeneration;
            impl_->geometryGeneration = geometryGeneration;
            impl_->itemClosed = false;
            impl_->hasOwnedFrame = false;
            impl_->lastPacket = {};
            impl_->lastHr = S_OK;
            impl_->policy.SetSessionGeneration(targetGeneration);
        }

        impl_->arrivedToken = impl_->framePool.FrameArrived({impl_.get(), &Impl::OnFrameArrived});
        impl_->closedToken = impl_->item.Closed({impl_.get(), &Impl::OnClosed});
        {
            std::lock_guard<std::mutex> const lock(impl_->mutex);
            impl_->policy.Apply(CaptureSessionEvent::StartSucceeded);
        }
        impl_->session.StartCapture();
        return true;
    }
    catch (winrt::hresult_error const& hrError)
    {
        impl_->lastHr = hrError.code();
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        error = L"WGC Start failed HRESULT=" + FormatHresult(impl_->lastHr);
        Stop();
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        return false;
    }
    catch (...)
    {
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        error = L"WGC Start failed with an unknown exception.";
        Stop();
        impl_->policy.Apply(CaptureSessionEvent::StartFailed);
        return false;
    }
}

void CaptureSession::Stop()
{
    if (!impl_)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> const lock(impl_->mutex);
        impl_->notifyWindow = nullptr;
        impl_->policy.Apply(CaptureSessionEvent::Stop);
    }
    impl_->TeardownResources();
}

void CaptureSession::OnItemClosed()
{
    if (!impl_)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> const lock(impl_->mutex);
        impl_->itemClosed = true;
        impl_->notifyWindow = nullptr;
        impl_->policy.Apply(CaptureSessionEvent::ItemClosed);
    }
    impl_->TeardownResources();
}

void CaptureSession::PumpHandoff()
{
    if (!impl_)
    {
        return;
    }

    std::vector<Impl::PendingItem> obsolete;
    Impl::PendingItem latest{};
    bool haveLatest = false;
    {
        std::lock_guard<std::mutex> const lock(impl_->mutex);
        while (impl_->pending.size() > 1)
        {
            obsolete.push_back(std::move(impl_->pending.front()));
            impl_->pending.pop_front();
            impl_->policy.NoteDequeued();
        }
        if (!impl_->pending.empty())
        {
            latest = std::move(impl_->pending.front());
            impl_->pending.pop_front();
            impl_->policy.NoteDequeued();
            haveLatest = true;
        }
    }

    for (auto& queued : obsolete)
    {
        CloseFrameQuiet(queued.frame);
    }
    if (!haveLatest)
    {
        return;
    }

    HRESULT copyHr = S_OK;
    std::wstring copyError;
    bool const copied = impl_->CopyOwned(latest, copyHr, copyError);

    auto abortOnDeviceLost = [this](HRESULT hr) -> bool {
        HWND notify = nullptr;
        UINT message = 0;
        HRESULT removed = S_OK;
        graphics::DeviceResources* gpu = nullptr;
        {
            std::lock_guard<std::mutex> const lock(impl_->mutex);
            gpu = impl_->device;
            notify = impl_->notifyWindow;
            message = impl_->notifyMessage;
        }
        bool const lost =
            (gpu != nullptr && gpu->CheckDeviceRemoved(removed)) || IsDxgiDeviceLost(hr);
        if (!lost)
        {
            return false;
        }
        {
            std::lock_guard<std::mutex> const lock(impl_->mutex);
            impl_->lastHr = FAILED(hr) ? hr : (removed != S_OK ? removed : DXGI_ERROR_DEVICE_REMOVED);
        }
        Stop();
        if (notify != nullptr && message != 0)
        {
            PostMessageW(notify, message, 2, 0);
        }
        return true;
    };
    if (abortOnDeviceLost(copyHr))
    {
        return;
    }

    std::wstring roiError;
    std::wstring lastRoiSrc;
    bool haveRoiSrc = false;
    std::wstring lastNavRoiSrc = L"none";
    bool haveNavRoiSrc = false;
    bool navigatorCopyFailed = false;
    bool navigatorApplied = false;
    bool evaluatedNavigator = false;
    RoiCpuSnapshot navigatorSnapshot{};
    bool haveNavigatorSnapshot = false;
    if (copied && impl_->ownedTexture)
    {
        graphics::DeviceResources* gpu = nullptr;
        CanvasRoiRequest roiRequest{};
        NavigatorRoiRequest navigatorRequest{};
        {
            std::lock_guard<std::mutex> const lock(impl_->mutex);
            gpu = impl_->device;
            roiRequest = impl_->canvasRoi;
            navigatorRequest = impl_->navigatorRoi;
        }
        if (gpu != nullptr && gpu->IsReady() && gpu->Device() != nullptr &&
            gpu->ImmediateContext() != nullptr)
        {
            RoiPixelRect requested{};
            std::wstring roiSrc = L"full-content-fallback";
            if (roiRequest.applied && roiRequest.mappingValidated && roiRequest.captureW > 0 &&
                roiRequest.captureH > 0)
            {
                requested.x = roiRequest.captureX;
                requested.y = roiRequest.captureY;
                requested.w = roiRequest.captureW;
                requested.h = roiRequest.captureH;
                roiSrc = L"applied-mapped";
            }
            else
            {
                requested.w = static_cast<int>(impl_->ownedWidth);
                requested.h = static_cast<int>(impl_->ownedHeight);
            }
            int const downsample = DefaultRoiDownsample(requested.w, requested.h);
            RoiBufferMeta meta{};
            meta.sequence = latest.packet.sequence;
            meta.captureTicks = latest.packet.captureTicks;
            meta.targetGeneration = latest.packet.targetGeneration;
            meta.geometryGeneration = latest.packet.geometryGeneration;
            meta.kind = RoiKind::Canvas;
            impl_->roiReadback.SubmitCopy(
                gpu->Device(),
                gpu->ImmediateContext(),
                impl_->ownedTexture.Get(),
                RoiKind::Canvas,
                requested,
                downsample,
                meta,
                roiError);
            impl_->roiReadback.TryComplete(gpu->ImmediateContext(), roiError);
            lastRoiSrc = std::move(roiSrc);
            haveRoiSrc = true;

            evaluatedNavigator = true;
            navigatorApplied = navigatorRequest.applied && navigatorRequest.mappingValidated &&
                               navigatorRequest.captureW > 0 && navigatorRequest.captureH > 0;
            if (navigatorApplied)
            {
                lastNavRoiSrc = L"applied-mapped";
                haveNavRoiSrc = true;
                RoiPixelRect navRequested{};
                navRequested.x = navigatorRequest.captureX;
                navRequested.y = navigatorRequest.captureY;
                navRequested.w = navigatorRequest.captureW;
                navRequested.h = navigatorRequest.captureH;
                int const navDownsample = DefaultRoiDownsample(navRequested.w, navRequested.h);
                RoiBufferMeta navMeta = meta;
                navMeta.kind = RoiKind::Navigator;
                std::wstring navError;
                bool const submitted = impl_->navigatorReadback.SubmitCopy(
                    gpu->Device(),
                    gpu->ImmediateContext(),
                    impl_->ownedTexture.Get(),
                    RoiKind::Navigator,
                    navRequested,
                    navDownsample,
                    navMeta,
                    navError);
                impl_->navigatorReadback.TryComplete(gpu->ImmediateContext(), navError);
                if (!submitted)
                {
                    navigatorCopyFailed = true;
                }
                else if (impl_->navigatorReadback.HasBuffer())
                {
                    navigatorSnapshot = PackRoiSnapshot(impl_->navigatorReadback.LastBuffer());
                    haveNavigatorSnapshot = navigatorSnapshot.valid;
                }
            }
            else
            {
                haveNavRoiSrc = true;
            }
        }
    }

    HRESULT recreateHr = S_OK;
    std::wstring recreateError;
    bool const recreated = impl_->RecreatePoolIfContentSizeChanged(
        latest.packet.contentWidth,
        latest.packet.contentHeight,
        recreateHr,
        recreateError);
    if (abortOnDeviceLost(recreateHr))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> const lock(impl_->mutex);
        impl_->lastHr = copied ? recreateHr : copyHr;
        impl_->lastRecreateHr = recreateHr;
        impl_->roiReport = impl_->roiReadback.FormatReport();
        impl_->navigatorRoiReport = impl_->navigatorReadback.FormatReport();
        if (haveRoiSrc)
        {
            impl_->roiSrc = std::move(lastRoiSrc);
        }
        if (haveNavRoiSrc)
        {
            impl_->navigatorRoiSrc = std::move(lastNavRoiSrc);
        }
        if (impl_->roiReadback.HasBuffer())
        {
            impl_->lastRoiBuffer = PackRoiSnapshot(impl_->roiReadback.LastBuffer());
            impl_->hasRoiBuffer = impl_->lastRoiBuffer.valid;
        }
        if (haveNavigatorSnapshot)
        {
            impl_->lastNavigatorRoiBuffer = std::move(navigatorSnapshot);
            impl_->hasNavigatorRoiBuffer = impl_->lastNavigatorRoiBuffer.valid;
        }
        else if (evaluatedNavigator && (!navigatorApplied || navigatorCopyFailed))
        {
            impl_->lastNavigatorRoiBuffer = {};
            impl_->hasNavigatorRoiBuffer = false;
        }
        if (copied)
        {
            impl_->lastPacket = latest.packet;
            impl_->lastPacket.stale = false;
            impl_->hasOwnedFrame = true;
        }
        else
        {
            impl_->hasOwnedFrame = false;
        }
    }
    (void)recreated;
    (void)copyError;
    (void)recreateError;
    (void)roiError;
}

CaptureSessionState CaptureSession::State() const noexcept
{
    if (!impl_)
    {
        return CaptureSessionState::Idle;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->policy.State();
}

FramePacket CaptureSession::LastPacket() const
{
    if (!impl_)
    {
        return {};
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->lastPacket;
}

bool CaptureSession::HasOwnedFrame() const noexcept
{
    if (!impl_)
    {
        return false;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->hasOwnedFrame;
}

bool CaptureSession::HasRoiBuffer() const noexcept
{
    if (!impl_)
    {
        return false;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->hasRoiBuffer;
}

RoiCpuSnapshot CaptureSession::LastRoiBuffer() const
{
    if (!impl_)
    {
        return {};
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->lastRoiBuffer;
}

bool CaptureSession::HasNavigatorRoiBuffer() const noexcept
{
    if (!impl_)
    {
        return false;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->hasNavigatorRoiBuffer;
}

RoiCpuSnapshot CaptureSession::LastNavigatorRoiBuffer() const
{
    if (!impl_)
    {
        return {};
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->lastNavigatorRoiBuffer;
}

void CaptureSession::NoteGeometryGeneration(std::uint64_t geometryGeneration) noexcept
{
    if (!impl_)
    {
        return;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    impl_->geometryGeneration = geometryGeneration;
}

void CaptureSession::SetCanvasRoiRequest(CanvasRoiRequest const& request) noexcept
{
    if (!impl_)
    {
        return;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    impl_->canvasRoi = request;
}

void CaptureSession::SetNavigatorRoiRequest(NavigatorRoiRequest const& request) noexcept
{
    if (!impl_)
    {
        return;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    impl_->navigatorRoi = request;
}

ID3D11Texture2D* CaptureSession::BorrowOwnedTexture() const noexcept
{
    if (!impl_)
    {
        return nullptr;
    }
    std::lock_guard<std::mutex> const lock(impl_->mutex);
    return impl_->ownedTexture.Get();
}

std::wstring CaptureSession::FormatReport() const
{
    if (!impl_)
    {
        return L"WGC session=(none)";
    }

    CaptureSessionPolicy policySnapshot;
    FramePacket packet{};
    HRESULT lastHr = S_OK;
    bool supported = false;
    bool supportChecked = false;
    bool itemClosed = false;
    bool hasOwned = false;
    std::uint64_t targetGeneration = 0;
    std::uint64_t geometryGeneration = 0;
    int poolWidth = 0;
    int poolHeight = 0;
    std::uint64_t recreateCount = 0;
    HRESULT lastRecreateHr = S_OK;
    std::wstring roiReport;
    std::wstring roiSrc;
    std::wstring navigatorRoiReport;
    std::wstring navigatorRoiSrc;
    {
        std::lock_guard<std::mutex> const lock(impl_->mutex);
        policySnapshot = impl_->policy;
        packet = impl_->lastPacket;
        lastHr = impl_->lastHr;
        supported = impl_->supported;
        supportChecked = impl_->supportChecked;
        itemClosed = impl_->itemClosed;
        hasOwned = impl_->hasOwnedFrame;
        targetGeneration = impl_->targetGeneration;
        geometryGeneration = impl_->geometryGeneration;
        poolWidth = impl_->poolWidth;
        poolHeight = impl_->poolHeight;
        recreateCount = impl_->recreateCount;
        lastRecreateHr = impl_->lastRecreateHr;
        roiReport = impl_->roiReport;
        roiSrc = impl_->roiSrc;
        navigatorRoiReport = impl_->navigatorRoiReport;
        navigatorRoiSrc = impl_->navigatorRoiSrc;
    }

    std::wstring supportText = L"unchecked";
    if (supportChecked)
    {
        supportText = supported ? L"yes" : L"no";
    }

    return L"WGC supported=" + supportText + L" state=" + FormatCaptureSessionState(policySnapshot.State()) +
           L" sessionGen=" + std::to_wstring(policySnapshot.SessionGeneration()) +
           L" targetGen=" + std::to_wstring(targetGeneration) + L" geomGen=" +
           std::to_wstring(geometryGeneration) + L"\r\n" + L"accepted=" +
           std::to_wstring(policySnapshot.Accepted()) + L" droppedBound=" +
           std::to_wstring(policySnapshot.DroppedBound()) + L" staleZero=" +
           std::to_wstring(policySnapshot.Stale()) + L" rejectedGen=" +
           std::to_wstring(policySnapshot.RejectedGeneration()) + L" pending=" +
           std::to_wstring(policySnapshot.Pending()) + L" lastSeq=" +
           std::to_wstring(packet.sequence) + L" contentSize=" +
           std::to_wstring(packet.contentWidth) + L"x" + std::to_wstring(packet.contentHeight) +
           L" captureTicks=" + std::to_wstring(packet.captureTicks) + L" stale=" +
           (packet.stale ? L"yes" : L"no") + L"\r\n" +            L"itemClosed=" + (itemClosed ? L"yes" : L"no") +
           L" ownedFrame=" + (hasOwned ? L"yes" : L"no") + L" lastHr=" + FormatHresult(lastHr) +
           L"\r\n" + L"poolSize=" + std::to_wstring(poolWidth) + L"x" + std::to_wstring(poolHeight) +
           L" recreates=" + std::to_wstring(recreateCount) + L" recreateHr=" +
           FormatHresult(lastRecreateHr) + L"\r\n" + roiReport + L" roiSrc=" + roiSrc + L"\r\n" +
           navigatorRoiReport + L" navRoiSrc=" + navigatorRoiSrc +
           L" (WGC copies owned textures; ROI CPU buffer is a packed copy, not zero-copy)";
}

} // namespace tracing::capture
