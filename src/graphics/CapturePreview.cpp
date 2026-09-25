#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "graphics/CapturePreview.h"

#include <cstdio>
#include <cstring>

#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif

namespace tracing::graphics {
namespace {

constexpr wchar_t kPreviewClass[] = L"TracingAppCapturePreview";
constexpr unsigned kMaxDimension = 16384;
constexpr unsigned kDefaultPreviewWidth = 640;
constexpr unsigned kDefaultPreviewHeight = 480;

std::wstring FormatHresult(HRESULT value)
{
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(value));
    return buffer;
}

bool PixelLooksMagenta(std::uint8_t blue, std::uint8_t green, std::uint8_t red) noexcept
{
    return red >= 240 && green <= 16 && blue >= 240;
}

} // namespace

CapturePreview::~CapturePreview()
{
    Release();
}

bool CapturePreview::RegisterPreviewClass(HINSTANCE instance, std::wstring& error)
{
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, kPreviewClass, &existing) != FALSE)
    {
        return true;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kPreviewClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        error = L"RegisterClassExW CapturePreview failed (Win32 " + std::to_wstring(GetLastError()) +
                L").";
        return false;
    }
    return true;
}

bool CapturePreview::CreatePreviewWindow(HINSTANCE instance, HWND owner, std::wstring& error)
{
    hwnd_ = CreateWindowExW(
        0,
        kPreviewClass,
        L"TracingApp Capture Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        static_cast<int>(kDefaultPreviewWidth),
        static_cast<int>(kDefaultPreviewHeight),
        owner,
        nullptr,
        instance,
        this);
    if (hwnd_ == nullptr)
    {
        error = L"CreateWindowExW CapturePreview failed (Win32 " + std::to_wstring(GetLastError()) +
                L").";
        return false;
    }

    BOOL const affinity = SetWindowDisplayAffinity(hwnd_, WDA_NONE);
    if (affinity == FALSE)
    {
        unsigned long const code = GetLastError();
        error = L"SetWindowDisplayAffinity(WDA_NONE) failed (Win32 " + std::to_wstring(code) + L").";
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
    return true;
}

bool CapturePreview::Create(HWND owner, DeviceResources& device, std::wstring& error)
{
    error.clear();
    Release();
    if (!device.IsReady() || device.Device() == nullptr)
    {
        error = L"CapturePreview requires a ready D3D11 device.";
        return false;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterPreviewClass(instance, error))
    {
        return false;
    }
    if (!CreatePreviewWindow(instance, owner, error))
    {
        return false;
    }

    device_ = &device;
    owner_ = owner;
    enabled_ = false;
    return true;
}

void CapturePreview::ReleaseCpuSurfaces() noexcept
{
    staging_.Reset();
    stagingFormat_ = DXGI_FORMAT_UNKNOWN;
    ReleaseDib();
}

void CapturePreview::ReleaseDib() noexcept
{
    if (dibDc_ != nullptr && dibOld_ != nullptr)
    {
        SelectObject(dibDc_, dibOld_);
        dibOld_ = nullptr;
    }
    if (dibBitmap_ != nullptr)
    {
        DeleteObject(dibBitmap_);
        dibBitmap_ = nullptr;
    }
    if (dibDc_ != nullptr)
    {
        DeleteDC(dibDc_);
        dibDc_ = nullptr;
    }
    dibBits_ = nullptr;
    bitmapWidth_ = 0;
    bitmapHeight_ = 0;
    hasBitmap_ = false;
}

void CapturePreview::Release()
{
    if (hwnd_ != nullptr)
    {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    ReleaseCpuSurfaces();
    device_ = nullptr;
    owner_ = nullptr;
    enabled_ = false;
    visible_ = false;
    markerFeedback_ = MarkerFeedback::NotSampled;
    label_ = CapturePreviewLabel::NoFrame;
}

void CapturePreview::SetEnabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled)
    {
        Hide();
        markerFeedback_ = MarkerFeedback::NotSampled;
        return;
    }
    if (hwnd_ != nullptr)
    {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
        UpdateTitle();
    }
}

void CapturePreview::Hide()
{
    if (hwnd_ != nullptr)
    {
        ShowWindow(hwnd_, SW_HIDE);
    }
    visible_ = false;
}

bool CapturePreview::EnsureCpuSurfaces(
    unsigned width,
    unsigned height,
    DXGI_FORMAT format,
    std::wstring& error)
{
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension)
    {
        error = L"CapturePreview size is invalid.";
        return false;
    }
    if (staging_ && dibBits_ != nullptr && bitmapWidth_ == width && bitmapHeight_ == height &&
        stagingFormat_ == format)
    {
        return true;
    }

    ReleaseCpuSurfaces();
    if (!device_ || device_->Device() == nullptr)
    {
        error = L"CapturePreview staging needs a ready D3D11 device.";
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    HRESULT hr = device_->Device()->CreateTexture2D(&stagingDesc, nullptr, &staging_);
    if (FAILED(hr) || !staging_)
    {
        error = L"CreateTexture2D preview staging failed HRESULT=" + FormatHresult(hr);
        return false;
    }
    stagingFormat_ = format;

    HDC const screen = GetDC(nullptr);
    if (screen == nullptr)
    {
        error = L"GetDC for preview DIB failed (Win32 " + std::to_wstring(GetLastError()) + L").";
        ReleaseCpuSurfaces();
        return false;
    }
    dibDc_ = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (dibDc_ == nullptr)
    {
        error = L"CreateCompatibleDC preview failed (Win32 " + std::to_wstring(GetLastError()) + L").";
        ReleaseCpuSurfaces();
        return false;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    dibBitmap_ = CreateDIBSection(dibDc_, &info, DIB_RGB_COLORS, &dibBits_, nullptr, 0);
    if (dibBitmap_ == nullptr || dibBits_ == nullptr)
    {
        unsigned long const code = GetLastError();
        ReleaseCpuSurfaces();
        error = L"CreateDIBSection preview failed (Win32 " + std::to_wstring(code) + L").";
        return false;
    }
    dibOld_ = SelectObject(dibDc_, dibBitmap_);
    bitmapWidth_ = width;
    bitmapHeight_ = height;
    return true;
}

bool CapturePreview::CopyTextureToDib(
    ID3D11Texture2D* ownedTexture,
    unsigned width,
    unsigned height,
    std::wstring& error)
{
    if (!device_ || !device_->IsReady() || device_->ImmediateContext() == nullptr || ownedTexture == nullptr)
    {
        error = L"CapturePreview copy skipped: device or texture missing.";
        return false;
    }

    ID3D11DeviceContext* const context = device_->ImmediateContext();
    context->CopyResource(staging_.Get(), ownedTexture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT const hr = context->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        error = L"Map preview staging failed HRESULT=" + FormatHresult(hr);
        return false;
    }

    auto* const dest = static_cast<std::uint8_t*>(dibBits_);
    auto const* const src = static_cast<std::uint8_t const*>(mapped.pData);
    UINT const rowBytes = width * 4;
    for (unsigned y = 0; y < height; ++y)
    {
        std::memcpy(dest + (static_cast<size_t>(y) * rowBytes), src + (static_cast<size_t>(y) * mapped.RowPitch), rowBytes);
    }
    context->Unmap(staging_.Get(), 0);
    hasBitmap_ = true;
    return true;
}

void CapturePreview::ScanMarkerFeedback(CaptureToClientMapping const& mapping)
{
    markerFeedback_ = MarkerFeedback::NotSampled;
    if (!hasBitmap_ || dibBits_ == nullptr || bitmapWidth_ == 0 || bitmapHeight_ == 0)
    {
        return;
    }

    long const originX = mapping.captureToClientX + kOverlayMarkerClientX;
    long const originY = mapping.captureToClientY + kOverlayMarkerClientY;
    if (originX < 0 || originY < 0)
    {
        markerFeedback_ = MarkerFeedback::Absent;
        return;
    }

    unsigned const sampleW = 16;
    unsigned const sampleH = 16;
    unsigned hit = 0;
    unsigned checked = 0;
    auto const* const pixels = static_cast<std::uint8_t const*>(dibBits_);
    UINT const stride = bitmapWidth_ * 4;
    for (unsigned y = 0; y < sampleH; ++y)
    {
        unsigned const py = static_cast<unsigned>(originY) + y;
        if (py >= bitmapHeight_)
        {
            break;
        }
        for (unsigned x = 0; x < sampleW; ++x)
        {
            unsigned const px = static_cast<unsigned>(originX) + x;
            if (px >= bitmapWidth_)
            {
                break;
            }
            std::uint8_t const* const pixel = pixels + (static_cast<size_t>(py) * stride) + (static_cast<size_t>(px) * 4);
            ++checked;
            if (PixelLooksMagenta(pixel[0], pixel[1], pixel[2]))
            {
                ++hit;
            }
        }
    }

    if (checked == 0)
    {
        markerFeedback_ = MarkerFeedback::NotSampled;
        return;
    }
    markerFeedback_ = hit > 0 ? MarkerFeedback::Present : MarkerFeedback::Absent;
}

void CapturePreview::UpdateTitle()
{
    if (hwnd_ == nullptr)
    {
        return;
    }
    std::wstring title = L"TracingApp Capture Preview [";
    title += FormatCapturePreviewLabel(label_);
    title += L"] seq=";
    title += std::to_wstring(sequence_);
    title += L" ticks=";
    title += std::to_wstring(captureTicks_);
    title += L" size=";
    title += std::to_wstring(bitmapWidth_);
    title += L"x";
    title += std::to_wstring(bitmapHeight_);
    title += L" markerFeedback=";
    title += FormatMarkerFeedback(markerFeedback_);
    SetWindowTextW(hwnd_, title.c_str());
}

void CapturePreview::Paint(HDC hdc, RECT const& client) const
{
    int const destW = client.right - client.left;
    int const destH = client.bottom - client.top;
    if (destW <= 0 || destH <= 0)
    {
        return;
    }

    if (!hasBitmap_ || dibDc_ == nullptr || bitmapWidth_ == 0 || bitmapHeight_ == 0)
    {
        HBRUSH const brush = CreateSolidBrush(RGB(48, 48, 48));
        FillRect(hdc, &client, brush);
        DeleteObject(brush);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(220, 220, 220));
        RECT fill = client;
        DrawTextW(
            hdc,
            FormatCapturePreviewLabel(label_),
            -1,
            &fill,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    SetStretchBltMode(hdc, HALFTONE);
    StretchBlt(
        hdc,
        0,
        0,
        destW,
        destH,
        dibDc_,
        0,
        0,
        static_cast<int>(bitmapWidth_),
        static_cast<int>(bitmapHeight_),
        SRCCOPY);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 220, 0));
    RECT banner{8, 8, destW - 8, 36};
    std::wstring caption = FormatCapturePreviewLabel(label_);
    caption += L"  seq=";
    caption += std::to_wstring(sequence_);
    DrawTextW(hdc, caption.c_str(), -1, &banner, DT_LEFT | DT_TOP | DT_SINGLELINE);
}

bool CapturePreview::Present(
    ID3D11Texture2D* ownedTexture,
    CapturePreviewLabel label,
    std::uint64_t sequence,
    std::int64_t captureTicks,
    CaptureToClientMapping const& mapping,
    std::wstring& error)
{
    error.clear();
    label_ = label;
    sequence_ = sequence;
    captureTicks_ = captureTicks;

    if (!enabled_ || hwnd_ == nullptr)
    {
        Hide();
        return true;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;

    if (ownedTexture == nullptr || label == CapturePreviewLabel::NoFrame ||
        label == CapturePreviewLabel::ItemClosed || label == CapturePreviewLabel::ZeroSize)
    {
        hasBitmap_ = false;
        markerFeedback_ = MarkerFeedback::NotSampled;
        UpdateTitle();
        InvalidateRect(hwnd_, nullptr, TRUE);
        UpdateWindow(hwnd_);
        return true;
    }

    D3D11_TEXTURE2D_DESC desc{};
    ownedTexture->GetDesc(&desc);
    if (!EnsureCpuSurfaces(desc.Width, desc.Height, desc.Format, error))
    {
        hasBitmap_ = false;
        UpdateTitle();
        return false;
    }
    if (!CopyTextureToDib(ownedTexture, desc.Width, desc.Height, error))
    {
        hasBitmap_ = false;
        UpdateTitle();
        return false;
    }

    if (label == CapturePreviewLabel::ActualFrame)
    {
        ScanMarkerFeedback(mapping);
    }
    else
    {
        markerFeedback_ = MarkerFeedback::NotSampled;
    }

    UpdateTitle();
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
    return true;
}

std::wstring CapturePreview::FormatReport() const
{
    return L"preview enabled=" + std::wstring(enabled_ ? L"yes" : L"no") + L" visible=" +
           (visible_ ? L"yes" : L"no") + L" label=" + FormatCapturePreviewLabel(label_) +
           L" seq=" + std::to_wstring(sequence_) + L" bitmap=" + std::to_wstring(bitmapWidth_) +
           L"x" + std::to_wstring(bitmapHeight_) + L" affinity=WDA_NONE markerFeedback=" +
           FormatMarkerFeedback(markerFeedback_) + L" (debug only; off for recording)";
}

LRESULT CALLBACK CapturePreview::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CapturePreview* self = nullptr;
    if (message == WM_NCCREATE)
    {
        auto const* create = reinterpret_cast<CREATESTRUCTW const*>(lParam);
        self = static_cast<CapturePreview*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr)
        {
            self->hwnd_ = hwnd;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    self = reinterpret_cast<CapturePreview*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC const hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        self->Paint(hdc, client);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        self->SetEnabled(false);
        return 0;
    case WM_DESTROY:
        self->hwnd_ = nullptr;
        self->visible_ = false;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace tracing::graphics
