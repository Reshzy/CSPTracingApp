#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

bool IsValidControlViewport(int width, int height) noexcept
{
    return width > 0 && height > 0;
}

#ifndef TRACING_APP_TESTING

#include <Unknwn.h>
#include <winrt/base.h>

#include <shobjidl.h>
#include <wrl/client.h>

#include "capture/CaptureSession.h"
#include "graphics/CapturePreview.h"
#include "graphics/DeviceResources.h"
#include "graphics/OverlaySurface.h"
#include "image/ImageLoader.h"
#include "platform/TargetDiscovery.h"
#include "platform/TargetGeometry.h"

namespace {
constexpr int kDefaultWidth = 720;
constexpr int kDefaultHeight = 840;
constexpr wchar_t kWindowClass[] = L"TracingAppControlWindow";
constexpr wchar_t kWindowTitle[] = L"TracingApp";
constexpr int kIdList = 1001;
constexpr int kIdRefresh = 1002;
constexpr int kIdSelect = 1003;
constexpr int kIdStatus = 1004;
constexpr int kIdEmergencyHide = 1005;
constexpr int kIdShowMarker = 1006;
constexpr int kIdTracingMode = 1007;
constexpr int kIdAlignmentMode = 1008;
constexpr int kIdCoverProbe = 1009;
constexpr int kIdStartCapture = 1010;
constexpr int kIdStopCapture = 1011;
constexpr int kIdEnablePreview = 1012;
constexpr int kIdImportImage = 1013;
constexpr UINT kMsgGeometry = WM_APP + 1;
constexpr UINT kMsgCapture = WM_APP + 2;
constexpr UINT kMsgStartCapture = WM_APP + 3;
constexpr UINT kMsgStopCapture = WM_APP + 4;
constexpr UINT_PTR kTimerGeometry = 1;
constexpr UINT kGeometryPollMs = 250;

struct ControlState
{
    HWND control = nullptr;
    HWND list = nullptr;
    HWND status = nullptr;
    std::vector<tracing::platform::WindowCandidate> candidates;
    std::optional<tracing::platform::TargetIdentity> selected;
    tracing::platform::TargetLifecycleWatcher watcher;
    tracing::platform::GeometrySnapshot lastGeometry;
    tracing::graphics::DeviceResources device;
    tracing::graphics::OverlaySurface overlay;
    tracing::graphics::CapturePreview preview;
    tracing::capture::CaptureSession capture;
    tracing::image::LoadedImageSlot image;
    HWND previewCheck = nullptr;
    bool hideOverlayOnCaptureLoss = false;
    std::uint64_t nextGeneration = 1;
};

std::wstring ControlDpiLine(HWND hwnd)
{
    unsigned const dpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : 0;
    return L"control DPI=" + std::to_wstring(dpi) +
           L" (physical, PerMonitorV2; not canvas bounds)\r\n";
}

tracing::graphics::CaptureToClientMapping MappingFromState(ControlState const& state)
{
    tracing::capture::FramePacket const packet = state.capture.LastPacket();
    tracing::platform::PhysicalRect const& client = state.lastGeometry.clientPhysical;
    tracing::platform::PhysicalRect const& outer = state.lastGeometry.outerWindow;
    tracing::platform::PhysicalRect const& dwm = state.lastGeometry.dwmFrame;
    return tracing::graphics::MeasureCaptureToClientMapping(
        packet.contentWidth,
        packet.contentHeight,
        client.x,
        client.y,
        client.width,
        client.height,
        outer.x,
        outer.y,
        outer.width,
        outer.height,
        dwm.x,
        dwm.y,
        dwm.width,
        dwm.height);
}

void SyncPreviewCheckbox(ControlState& state)
{
    if (state.previewCheck != nullptr)
    {
        SendMessageW(
            state.previewCheck,
            BM_SETCHECK,
            state.preview.IsEnabled() ? BST_CHECKED : BST_UNCHECKED,
            0);
    }
}

void PresentPreview(ControlState& state)
{
    SyncPreviewCheckbox(state);
    if (!state.preview.IsEnabled())
    {
        return;
    }

    tracing::capture::FramePacket const packet = state.capture.LastPacket();
    bool const itemClosed =
        state.capture.State() == tracing::capture::CaptureSessionState::ItemClosed;
    tracing::graphics::CapturePreviewLabel const label = tracing::graphics::ClassifyCapturePreviewLabel(
        state.capture.HasOwnedFrame(),
        packet.stale,
        packet.contentWidth,
        packet.contentHeight,
        itemClosed);
    std::wstring error;
    state.preview.Present(
        state.capture.BorrowOwnedTexture(),
        label,
        packet.sequence,
        packet.captureTicks,
        MappingFromState(state),
        error);
}

std::wstring StatusHeader(ControlState const& state)
{
    return ControlDpiLine(state.control) + state.device.FormatReport() + L"\r\n" +
           state.overlay.FormatReport() + L"\r\n" + state.capture.FormatReport() + L"\r\n" +
           state.preview.FormatReport() + L"\r\n" +
           tracing::graphics::FormatCaptureToClientMapping(MappingFromState(state)) + L"\r\n" +
           state.image.FormatReport() + L"\r\n" +
           L"overlayHiddenOnCaptureLoss=" +
           std::wstring(state.hideOverlayOnCaptureLoss ? L"yes" : L"no") + L"\r\n";
}

void SetStatus(ControlState& state, std::wstring const& text)
{
    if (state.status != nullptr)
    {
        SetWindowTextW(state.status, text.c_str());
    }
}

void StopCapture(ControlState& state)
{
    state.capture.Stop();
    if (state.preview.IsEnabled())
    {
        std::wstring error;
        tracing::graphics::CaptureToClientMapping const mapping = MappingFromState(state);
        state.preview.Present(
            nullptr,
            tracing::graphics::CapturePreviewLabel::NoFrame,
            0,
            0,
            mapping,
            error);
    }
    else
    {
        state.preview.Hide();
    }
}

void StopWatching(ControlState& state)
{
    state.watcher.Detach();
    if (state.control != nullptr)
    {
        KillTimer(state.control, kTimerGeometry);
    }
    state.lastGeometry = {};
}

void SyncOverlayFromState(ControlState& state)
{
    tracing::graphics::OverlayPlacement placement{};
    bool targetUsable = false;
    bool targetForeground = false;
    bool const hasTarget = state.selected.has_value();
    bool const controlForeground =
        state.control != nullptr && GetForegroundWindow() == state.control;

    if (hasTarget)
    {
        tracing::platform::GeometrySnapshot const& geometry = state.lastGeometry;
        tracing::platform::OverlayEligibility const eligibility =
            tracing::platform::ClassifyOverlayEligibility(geometry);
        targetUsable = eligibility == tracing::platform::OverlayEligibility::Eligible ||
                       eligibility == tracing::platform::OverlayEligibility::NotForeground;
        targetForeground = geometry.targetForeground;
        placement.x = geometry.clientPhysical.x;
        placement.y = geometry.clientPhysical.y;
        placement.width = geometry.clientPhysical.width;
        placement.height = geometry.clientPhysical.height;
        if (state.hideOverlayOnCaptureLoss)
        {
            targetUsable = false;
        }
    }
    else if (state.overlay.TestPatternActive())
    {
        placement = state.overlay.LastPlacement();
    }

    std::wstring error;
    state.overlay.UpdatePlacementAndVisibility(
        placement,
        targetUsable,
        targetForeground,
        controlForeground,
        hasTarget,
        error);
}

void SetStatusWithOverlay(ControlState& state, std::wstring const& body)
{
    SyncPreviewCheckbox(state);
    SyncOverlayFromState(state);
    if (body.empty())
    {
        SetStatus(state, StatusHeader(state));
        return;
    }
    SetStatus(state, StatusHeader(state) + body);
}

void RefreshGeometryDisplay(ControlState& state, std::wstring const& extra)
{
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, extra);
        return;
    }

    tracing::platform::GeometrySnapshot sampled{};
    std::wstring error;
    bool const ok = tracing::platform::QueryPhysicalGeometry(*state.selected, sampled, error);
    if (!ok)
    {
        if (sampled.identity == tracing::platform::IdentityCheck::WindowDead ||
            sampled.identity == tracing::platform::IdentityCheck::HandleReused)
        {
            StopCapture(state);
            StopWatching(state);
            state.selected.reset();
            std::wstring body = error;
            if (!extra.empty())
            {
                body += L"\r\n";
                body += extra;
            }
            SetStatusWithOverlay(state, body);
            return;
        }
        std::wstring body = error + L"\r\n" + tracing::platform::FormatGeometryReport(sampled);
        if (!extra.empty())
        {
            body += L"\r\n";
            body += extra;
        }
        SetStatusWithOverlay(state, body);
        return;
    }

    sampled.geometryGeneration =
        tracing::platform::NextGeometryGeneration(state.lastGeometry, sampled);
    state.lastGeometry = sampled;
    state.capture.NoteGeometryGeneration(sampled.geometryGeneration);
    std::wstring body = tracing::platform::FormatGeometryReport(sampled);
    if (!extra.empty())
    {
        body += L"\r\n";
        body += extra;
    }
    SetStatusWithOverlay(state, body);
}

bool StartWatching(ControlState& state, std::wstring& hookError)
{
    hookError.clear();
    if (!state.selected.has_value() || state.control == nullptr)
    {
        hookError = L"Cannot watch geometry without a selected target.";
        return false;
    }

    StopWatching(state);
    bool const hooked = state.watcher.Attach(
        state.control,
        state.selected->hwnd,
        kMsgGeometry,
        hookError);
    SetTimer(state.control, kTimerGeometry, kGeometryPollMs, nullptr);
    return hooked;
}

void RefillList(ControlState& state)
{
    SendMessageW(state.list, LB_RESETCONTENT, 0, 0);
    for (std::size_t i = 0; i < state.candidates.size(); ++i)
    {
        tracing::platform::WindowCandidate const& candidate = state.candidates[i];
        if (candidate.kind == tracing::platform::CandidateKind::RejectedNotVisible)
        {
            continue;
        }
        std::wstring const line = tracing::platform::FormatCandidateLine(candidate);
        LRESULT const index = SendMessageW(
            state.list,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(line.c_str()));
        if (index != LB_ERR)
        {
            SendMessageW(state.list, LB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(i));
        }
    }
}

void RefreshCandidates(ControlState& state)
{
    std::wstring error;
    if (!tracing::platform::EnumerateTopLevelCandidates(state.candidates, error))
    {
        SetStatusWithOverlay(state, error);
        return;
    }
    RefillList(state);

    if (state.selected.has_value())
    {
        tracing::platform::TargetIdentity const& stored = *state.selected;
        bool const hwndAlive = IsWindow(stored.hwnd) != FALSE;
        tracing::platform::ProcessIdentity observed{};
        std::wstring accessError;
        bool observedKnown = false;
        if (hwndAlive)
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(stored.hwnd, &pid);
            observed.pid = pid;
            for (tracing::platform::WindowCandidate const& candidate : state.candidates)
            {
                if (candidate.hwnd == stored.hwnd)
                {
                    observed = candidate.process;
                    observedKnown = candidate.kind != tracing::platform::CandidateKind::AccessFailed &&
                                    candidate.process.creationTime != 0;
                    accessError = candidate.accessError;
                    break;
                }
            }
        }

        tracing::platform::IdentityCheck const check = tracing::platform::CheckIdentity(
            stored.process.pid,
            stored.process.creationTime,
            hwndAlive,
            observedKnown,
            observed.pid,
            observed.creationTime);
        if (check == tracing::platform::IdentityCheck::WindowDead)
        {
            StopCapture(state);
            StopWatching(state);
            state.selected.reset();
            SetStatusWithOverlay(
                state,
                L"Previous target is gone. HWND is no longer valid; geometry invalidated. "
                L"Select a painting window again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::HandleReused)
        {
            StopCapture(state);
            StopWatching(state);
            state.selected.reset();
            SetStatusWithOverlay(
                state,
                L"Previous HWND was reused by another process. Target and geometry cleared so "
                L"a launcher or second instance cannot silently replace it. Select again.");
            return;
        }
        if (check == tracing::platform::IdentityCheck::AccessFailed)
        {
            SetStatusWithOverlay(
                state,
                accessError.empty()
                    ? tracing::platform::FormatAccessFailure(
                          stored.process.pid, ERROR_ACCESS_DENIED)
                    : accessError);
            return;
        }

        RefreshGeometryDisplay(state, L"refreshed candidate list");
        return;
    }

    SetStatusWithOverlay(
        state,
        L"Refreshed top-level windows. Select a PAINT row (CLIPStudioPaint.exe), then Select. "
        L"Launcher rows are never chosen automatically.");
}

void SelectFromUi(ControlState& state)
{
    std::optional<std::size_t> userIndex;
    LRESULT const sel = SendMessageW(state.list, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR)
    {
        LRESULT const data = SendMessageW(state.list, LB_GETITEMDATA, static_cast<WPARAM>(sel), 0);
        if (data != LB_ERR)
        {
            userIndex = static_cast<std::size_t>(data);
        }
    }

    tracing::platform::TargetSelection const selection = tracing::platform::ResolveSelection(
        state.candidates,
        userIndex,
        state.nextGeneration);
    if (selection.status != tracing::platform::SelectionStatus::Selected)
    {
        SetStatusWithOverlay(state, selection.message);
        return;
    }

    StopCapture(state);
    StopWatching(state);
    state.selected = selection.identity;
    ++state.nextGeneration;

    std::wstring hookError;
    bool const hooked = StartWatching(state, hookError);
    std::wstring extra = selection.message;
    if (!hooked)
    {
        extra += L"\r\n";
        extra += hookError;
    }
    RefreshGeometryDisplay(state, extra);
}

void OnEmergencyHide(ControlState& state)
{
    state.overlay.EmergencyHide();
    SetStatusWithOverlay(
        state,
        L"Emergency hide latched. Overlay stays hidden until Show test marker.");
}

void OnShowTestMarker(ControlState& state)
{
    state.hideOverlayOnCaptureLoss = false;
    state.overlay.ClearEmergencyHide();
    if (!state.selected.has_value())
    {
        state.overlay.RequestTestPattern();
        SetStatusWithOverlay(
            state,
            L"Show test marker: test-pattern surface (not imported image).");
        return;
    }
    state.overlay.ClearTestPattern();
    RefreshGeometryDisplay(
        state,
        L"Show test marker: using selected target client physical bounds.");
}

void OnTracingMode(ControlState& state)
{
    state.overlay.SetInteractionMode(tracing::graphics::OverlayInteractionMode::Tracing);
    SetStatusWithOverlay(
        state,
        L"Interaction mode: tracing (noninteractive pass-through).");
}

void OnAlignmentMode(ControlState& state)
{
    state.overlay.SetInteractionMode(tracing::graphics::OverlayInteractionMode::Alignment);
    SetStatusWithOverlay(
        state,
        L"Interaction mode: alignment (overlay accepts input).");
}

void OnStartCapture(ControlState& state)
{
    if (state.control != nullptr)
    {
        PostMessageW(state.control, kMsgStartCapture, 0, 0);
    }
}

void OnStopCapture(ControlState& state)
{
    if (state.control != nullptr)
    {
        PostMessageW(state.control, kMsgStopCapture, 0, 0);
    }
}

void OnEnablePreview(ControlState& state)
{
    bool const checked =
        state.previewCheck != nullptr &&
        SendMessageW(state.previewCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.preview.SetEnabled(checked);
    if (checked)
    {
        PresentPreview(state);
        SetStatusWithOverlay(
            state,
            L"Capture preview enabled (ordinary WDA_NONE debug window; not for recording).");
        return;
    }
    SetStatusWithOverlay(state, L"Capture preview disabled (recording-safe default).");
}

void OnImportImage(ControlState& state)
{
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || !dialog)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: CoCreateInstance IFileOpenDialog failed hr=") + buffer);
        return;
    }

    COMDLG_FILTERSPEC const filters[] = {
        {L"PNG and JPEG", L"*.png;*.jpg;*.jpeg"},
        {L"PNG", L"*.png"},
        {L"JPEG", L"*.jpg;*.jpeg"},
    };
    dialog->SetFileTypes(ARRAYSIZE(filters), filters);
    dialog->SetTitle(L"Import reference PNG or JPEG");
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options)))
    {
        dialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    }

    hr = dialog->Show(state.control);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        SetStatusWithOverlay(state, L"Import image cancelled; previous image preserved.");
        return;
    }
    if (FAILED(hr))
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: IFileOpenDialog::Show failed hr=") + buffer);
        return;
    }

    Microsoft::WRL::ComPtr<IShellItem> item;
    hr = dialog->GetResult(&item);
    if (FAILED(hr) || !item)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: GetResult failed hr=") + buffer);
        return;
    }

    PWSTR filePath = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
    if (FAILED(hr) || filePath == nullptr)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned>(hr));
        SetStatusWithOverlay(
            state,
            std::wstring(L"Import image: GetDisplayName failed hr=") + buffer);
        return;
    }
    std::wstring const path(filePath);
    CoTaskMemFree(filePath);

    std::wstring error;
    if (state.image.TryLoad(path, error))
    {
        SetStatusWithOverlay(
            state,
            L"Imported image (WIC decode only; overlay still test-pattern, not uploaded).");
        return;
    }
    SetStatusWithOverlay(
        state,
        L"Import failed; previous image preserved.\r\n" + error);
}

void StartCaptureNow(ControlState& state)
{
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, L"Select a PAINT HWND before Start Capture.");
        return;
    }
    if (!state.device.IsReady())
    {
        SetStatusWithOverlay(state, L"Start Capture requires a ready D3D11 device.");
        return;
    }
    if (state.lastGeometry.identity != tracing::platform::IdentityCheck::Match)
    {
        RefreshGeometryDisplay(state, L"");
    }
    if (!state.selected.has_value())
    {
        SetStatusWithOverlay(state, L"Target became invalid before Start Capture.");
        return;
    }

    std::wstring error;
    if (!state.capture.Start(
            state.selected->hwnd,
            state.selected->sessionGeneration,
            state.lastGeometry.geometryGeneration,
            state.device,
            state.control,
            kMsgCapture,
            error))
    {
        SetStatusWithOverlay(state, error);
        return;
    }
    state.hideOverlayOnCaptureLoss = false;
    RefreshGeometryDisplay(
        state,
        L"WGC capture started (owned frames; preview off by default). Overlay test-pattern "
        L"is not a captured frame.");
}

std::wstring FormatMonitorDevice(POINT origin)
{
    HMONITOR const monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) == FALSE)
    {
        return L"monitor=unknown";
    }
    return std::wstring(L"monitor=") + info.szDevice;
}

std::wstring FormatVirtualDesktop()
{
    return L"virtualScreen origin=(" +
           std::to_wstring(GetSystemMetrics(SM_XVIRTUALSCREEN)) + L"," +
           std::to_wstring(GetSystemMetrics(SM_YVIRTUALSCREEN)) + L") size=" +
           std::to_wstring(GetSystemMetrics(SM_CXVIRTUALSCREEN)) + L"x" +
           std::to_wstring(GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

void OnCoverInputProbe(ControlState& state)
{
    HWND const probe = FindWindowW(L"TracingAppInputProbe", nullptr);
    if (probe == nullptr)
    {
        SetStatusWithOverlay(
            state,
            L"Input probe not running. Start TracingApp.InputProbe.exe first.");
        return;
    }

    RECT client{};
    if (GetClientRect(probe, &client) == FALSE || client.right <= client.left ||
        client.bottom <= client.top)
    {
        SetStatusWithOverlay(state, L"Input probe client rect is empty.");
        return;
    }

    POINT origin{client.left, client.top};
    if (ClientToScreen(probe, &origin) == FALSE)
    {
        SetStatusWithOverlay(state, L"ClientToScreen failed for input probe.");
        return;
    }

    StopCapture(state);
    StopWatching(state);
    state.selected.reset();
    state.overlay.ClearEmergencyHide();

    tracing::graphics::OverlayPlacement placement{};
    placement.x = origin.x;
    placement.y = origin.y;
    placement.width = client.right - client.left;
    placement.height = client.bottom - client.top;
    state.overlay.RequestTestPatternAt(placement);

    DWORD probePid = 0;
    GetWindowThreadProcessId(probe, &probePid);
    SetStatusWithOverlay(
        state,
        L"Cover input probe: test-pattern over TracingAppInputProbe PID " +
            std::to_wstring(probePid) +
            L" (different process; not a CSP target).\r\nprobe client origin=(" +
            std::to_wstring(origin.x) + L"," + std::to_wstring(origin.y) + L") size=" +
            std::to_wstring(placement.width) + L"x" + std::to_wstring(placement.height) +
            L"\r\n" + FormatMonitorDevice(origin) + L" " + FormatVirtualDesktop() +
            L"\r\nSendInput must use virtual-desktop absolute mapping, not primary-only.");
}

LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<ControlState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        auto* created = new ControlState();
        created->control = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
        created->list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            12,
            12,
            680,
            200,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdList)),
            instance,
            nullptr);
        created->status = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12,
            316,
            680,
            440,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatus)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Refresh",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            220,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRefresh)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Select",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            124,
            220,
            100,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSelect)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Emergency Hide",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            236,
            220,
            140,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEmergencyHide)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Show test marker",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            384,
            220,
            150,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShowMarker)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Tracing mode",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            12,
            252,
            130,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTracingMode)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Alignment mode",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            150,
            252,
            140,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAlignmentMode)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Cover input probe",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            298,
            252,
            160,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCoverProbe)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Start Capture",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            466,
            252,
            104,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStartCapture)),
            instance,
            nullptr);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Stop Capture",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            576,
            252,
            104,
            28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStopCapture)),
            instance,
            nullptr);
        created->previewCheck = CreateWindowExW(
            0,
            L"BUTTON",
            L"Enable capture preview",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            12,
            284,
            220,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEnablePreview)),
            instance,
            nullptr);
        SendMessageW(created->previewCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        CreateWindowExW(
            0,
            L"BUTTON",
            L"Import image",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            240,
            284,
            140,
            24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdImportImage)),
            instance,
            nullptr);
        std::wstring deviceError;
        if (!created->device.Create(deviceError))
        {
            OutputDebugStringW(deviceError.c_str());
            OutputDebugStringW(L"\r\n");
        }
        else
        {
            std::wstring overlayError;
            if (!created->overlay.Create(hwnd, created->device, overlayError))
            {
                OutputDebugStringW(overlayError.c_str());
                OutputDebugStringW(L"\r\n");
            }
            else
            {
                std::wstring previewError;
                if (!created->preview.Create(hwnd, created->device, previewError))
                {
                    OutputDebugStringW(previewError.c_str());
                    OutputDebugStringW(L"\r\n");
                }
            }
        }
        RefreshCandidates(*created);
        return 0;
    }
    case WM_COMMAND:
        if (state != nullptr)
        {
            int const id = LOWORD(wParam);
            int const code = HIWORD(wParam);
            if (id == kIdRefresh && code == BN_CLICKED)
            {
                RefreshCandidates(*state);
                return 0;
            }
            if (id == kIdSelect && code == BN_CLICKED)
            {
                SelectFromUi(*state);
                return 0;
            }
            if (id == kIdEmergencyHide && code == BN_CLICKED)
            {
                OnEmergencyHide(*state);
                return 0;
            }
            if (id == kIdShowMarker && code == BN_CLICKED)
            {
                OnShowTestMarker(*state);
                return 0;
            }
            if (id == kIdTracingMode && code == BN_CLICKED)
            {
                OnTracingMode(*state);
                return 0;
            }
            if (id == kIdAlignmentMode && code == BN_CLICKED)
            {
                OnAlignmentMode(*state);
                return 0;
            }
            if (id == kIdCoverProbe && code == BN_CLICKED)
            {
                OnCoverInputProbe(*state);
                return 0;
            }
            if (id == kIdStartCapture && code == BN_CLICKED)
            {
                OnStartCapture(*state);
                return 0;
            }
            if (id == kIdStopCapture && code == BN_CLICKED)
            {
                OnStopCapture(*state);
                return 0;
            }
            if (id == kIdEnablePreview && code == BN_CLICKED)
            {
                OnEnablePreview(*state);
                return 0;
            }
            if (id == kIdImportImage && code == BN_CLICKED)
            {
                OnImportImage(*state);
                return 0;
            }
            if (id == kIdList && code == LBN_DBLCLK)
            {
                SelectFromUi(*state);
                return 0;
            }
        }
        break;
    case kMsgGeometry:
        if (state != nullptr && state->selected.has_value())
        {
            RefreshGeometryDisplay(*state, L"WinEvent");
            return 0;
        }
        break;
    case kMsgCapture:
        if (state != nullptr)
        {
            if (wParam == 1)
            {
                state->capture.OnItemClosed();
                state->hideOverlayOnCaptureLoss = true;
                PresentPreview(*state);
                RefreshGeometryDisplay(*state, L"WGC item Closed; session torn down; tracing overlay hidden.");
                return 0;
            }
            state->capture.PumpHandoff();
            tracing::capture::FramePacket const packet = state->capture.LastPacket();
            if (packet.stale)
            {
                state->hideOverlayOnCaptureLoss = true;
            }
            PresentPreview(*state);
            RefreshGeometryDisplay(*state, L"");
            return 0;
        }
        break;
    case kMsgStartCapture:
        if (state != nullptr)
        {
            StartCaptureNow(*state);
            return 0;
        }
        break;
    case kMsgStopCapture:
        if (state != nullptr)
        {
            StopCapture(*state);
            SetStatusWithOverlay(*state, L"WGC capture stopped.");
            return 0;
        }
        break;
    case WM_TIMER:
        if (state != nullptr && wParam == kTimerGeometry && state->selected.has_value())
        {
            RefreshGeometryDisplay(*state, L"");
            return 0;
        }
        break;
    case WM_DPICHANGED:
        if (state != nullptr)
        {
            RefreshGeometryDisplay(*state, L"control DPI changed");
        }
        break;
    case WM_DESTROY:
        if (state != nullptr)
        {
            StopCapture(*state);
            StopWatching(*state);
            state->preview.Release();
            state->overlay.Release();
            state->device.Release();
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    if (!IsValidControlViewport(kDefaultWidth, kDefaultHeight))
    {
        return 1;
    }

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ControlWndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    HWND const window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kDefaultWidth,
        kDefaultHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

#endif
