# TracingApp — prototype master

Status: proposed implementation contract; hardware and application tests NOT RUN.
Date: 2026-09-25. Target: Windows desktop, x64, one CSP document view at a time.

## 1. Goal and user experience

Build an independent Windows utility that places an imported image over the Clip Studio Paint (CSP) canvas. The user can draw through the overlay, and the reference follows canvas pan, zoom, rotation, and flip when tracking is reliable. OBS **Display Capture** records CSP and an optional ordinary reference window while omitting the tracing overlay.

The two windows have different roles:

- `TracingOverlayWindow`: private tracing surface, transparent, nonactivating, click-through in tracing mode, capture exclusion applied before showing content.
- `ReferenceWindow`: ordinary visible reference image, independently movable, opacity normally 100%, `WDA_NONE`, intended to appear in the recording. It can initially share the control panel window.

Controls live in an ordinary Win32 control window. Provide target selection, image import, opacity, canvas/Navigator region selection, alignment, start/pause tracking, resync, emergency hide, and diagnostics. Alignment mode may deliberately accept input; tracing mode must not intercept drawing. Clearly indicate mode changes.

Start with manual calibration and manual transforms. Automatic synchronization is a later capability, not a prerequisite for proving the rendering/privacy pipeline.

## 2. Scope and constraints

MVP scope: one selected CSP top-level HWND; one reference image; one rectangular user-selected canvas viewport; one selected document; SDR desktop; Windows 11 x64 as the primary tested platform. Windows 10 version 2004 is the API floor for exclusion, not a promise of product support. Test additional OS versions explicitly before listing them as supported.

Include move/resize, mixed-DPI monitor movement, minimize/restore, focus changes, target close, document-switch invalidation, image load errors, capture loss, and device-loss recovery. Initially require a fixed CSP workspace layout and an unobscured Navigator for Navigator-assisted mode. Store calibration per session only until reliable document identity is demonstrated.

Out of scope: WinUI, Electron, web UI, MSIX/store packaging, cloud upload, streaming integration, CSP plug-in development, process injection, memory scanning, graphics API hooking inside CSP, driver code, automated drawing/input replay, generalized multi-document tracking, HDR fidelity, color-management parity, multi-touch prediction, or a universal promise of screenshot prevention.

Do not infer that the tracing reference resembles the painting. Track **CSP pixels against earlier CSP pixels**, not the imported reference against CSP. Do not modify the CSP document to insert fiducials. A blank canvas may be unobservable: retain manual mode or hide and request resync.

## 3. Stack and build contract

- C++23, using only features supported by the installed MSVC toolset. Avoid modules and experimental dependencies for this prototype.
- Plain Unicode Win32 windows and message loop; per-monitor DPI awareness V2 manifest.
- D3D11 for capture texture handling and image rendering; DXGI plus DirectComposition for a composition swap chain with premultiplied alpha. DirectComposition is a small compositor dependency, not WinUI.
- Windows Graphics Capture (WGC), accessed with C++/WinRT and Win32 capture-item interop. C++/WinRT does not require adopting Windows App SDK UI.
- `SetWindowDisplayAffinity(..., WDA_EXCLUDEFROMCAPTURE)` on the private top-level HWND only.
- WIC for decoding PNG/JPEG and converting pixels into a documented BGRA format.
- OpenCV for CPU visual tracking, introduced after capture/exclusion gates. Pin a tested package revision using a vcpkg manifest baseline. Do not assume OpenCV 5.x exists or is required; the needed algorithms are available in documented OpenCV 4.x APIs.
- GoogleTest and CTest for pure math, lifecycle logic, replay, and estimator tests.
- Optional observers: WinEvent for geometry/lifecycle; Raw Input for brief motion prediction; UI Automation for accessible values only where observed on the installed CSP build. No observer is authoritative by default.
- Visual Studio 2026 Community installed with Desktop development with C++, current supported MSVC x64/x86 tools, Windows SDK, and CMake tools. Cursor is the editing environment; Visual Studio is also available for native/GPU debugging.
- CMake 4.2+ for the documented `Visual Studio 18 2026` generator; use x64 presets named `windows-debug` and `windows-release`. Verify the installed generator with `cmake --help`. Ninja is an optional later preset with an initialized MSVC environment, not a second mandatory build path.

Use a vcpkg toolchain path derived from `VCPKG_ROOT`, never a committed personal path. Resolve and record an actual baseline; never invent a commit hash. Keep runtime library and dependency triplets consistent (`x64-windows` initially). Use SDK-provided C++/WinRT headers when sufficient; if generated projection support is needed, pin and document its actual package/tool version.

Canonical commands from repository root:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Build/test presets must explicitly select Debug or Release for the multi-configuration generator. A missing test executable or zero discovered tests is not a pass. Add a simple meaningful core test in the bootstrap. Dependency installation requires network availability; report failures honestly rather than generating placeholder results.

## 4. Architecture and module boundaries

Data flow:

```text
CSP HWND -> TargetSession -> WGC -> owned GPU frame / frame metadata
                                    |                 |
                                    |                 +-> debug preview (off by default)
                                    +-> bounded ROI readback -> visual / Navigator observations
WinEvent -> geometry snapshot ------------------------|
Raw Input / UIA -> optional timestamped observations --|
                                                      v
                                              TransformFusion
                                                      |
WIC image -> reference texture + reference placement --+-> OverlayRenderer
                                                      +-> diagnostics
Reference texture ---------------------------------------> ordinary ReferenceWindow
```

Boundaries and ownership:

1. `app/`: composition root, Win32 control UI, commands, session state. Owns startup/shutdown; coordinates modules through explicit interfaces. No CV algorithms here.
2. `platform/`: HWND discovery, process identity, physical screen geometry, DPI conversions, optional observers, display affinity. No renderer or transform math.
3. `graphics/`: D3D device/context ownership, composition targets, texture upload, draw and present. No guessing CSP state.
4. `capture/`: WGC creation/stop/recreate, texture copies and timestamps, target generation. No input interpretation.
5. `image/`: WIC decode, dimensions/orientation policy, validated image buffers. No window ownership.
6. `core/`: double-precision 2D transforms, typed coordinate spaces, calibration, quality/state policies. No Windows/OpenCV dependency; independently testable.
7. `tracking/`: ROI extraction, OpenCV algorithms, Navigator adapter, prediction/fusion and replay. Returns estimates plus confidence, never directly moves HWNDs.
8. `diagnostics/`: bounded metrics/events, local logs and optional replay recording. Image recording is opt-in.

Define minimal contracts only when first needed:

- `TargetIdentity`: HWND, PID, process creation time, session generation; HWND alone can be reused.
- `GeometrySnapshot`: generation, physical client origin/size, DPI, canvas ROI, capture-to-client mapping, visibility/foreground eligibility.
- `FramePacket`: owned texture slot, sequence, capture timestamp, content size, geometry generation, target generation. Never expose a released WGC frame surface as if independently owned.
- `Observation`: measurement timestamp, source, generation, measured transform/components, confidence details, validity reason.
- `TransformSnapshot`: canonical absolute transform, generation, observation age, tracking state, parity hypothesis, quality metrics.

Thread model: UI thread owns windows; one graphics owner serializes immediate-context work, upload, capture copies and rendering; one tracking worker processes latest ROI buffers. WGC free-threaded callbacks acquire frames and enqueue bounded work without performing CV. Synchronize frame ownership carefully if handoff delays copies. A small copy pool and at most two pending work items is enough; discard old work rather than building latency. No unsynchronized D3D11 immediate-context calls across threads.

On stop: invalidate generation, unsubscribe events, stop session/pool, drain or discard queued frames, stop tracking, release dependent textures/swap chains, then device and windows. Never join a worker while holding a lock needed by its callback. Device removal invalidates all device-dependent resources; hide, rebuild, then require fresh valid frames before resume.

## 5. CSP detection and geometry

Enumerate top-level windows using `EnumWindows`; inspect PID/executable identity, visibility, owner relationships, and actual candidate behavior. Do not rely solely on localized titles or a guessed fixed window class. Present candidates for explicit selection when ambiguous. Do not attach to the launcher by accident.

Record `GetClientRect` plus client-to-screen mapping and DWM frame information for diagnostics. Distinguish window bounds, client bounds, captured texture content bounds, and canvas bounds. Establish capture-to-client offsets experimentally with a known test window; do not assume WGC starts at the client origin. Track geometry generations so a resize cannot combine old frame pixels with a new origin.

Begin with user-selected canvas and Navigator ROIs. Reject empty/out-of-bounds rectangles, invalidate after layout changes, and provide recalibration. Clip private image rendering to the canvas ROI; do not draw over CSP toolbars, tabs, or floating panels.

Use a foreground/visibility policy rather than permanent global topmost behavior. Hide when CSP is minimized, closed, cloaked, unrelated apps are foreground, or session geometry is invalid. Account for the app's own control window intentionally receiving focus. CSP modal dialogs and overlapping floating palettes must either be detected and excluded or cause a pause; a bounding rectangle is not an occlusion model.

## 6. Transparent overlay and rendering proof

Create a top-level popup in our process, nonactivating/tool-window behavior, no taskbar entry, and explicitly managed z-order. Use transparent clear color and premultiplied-alpha composition. Test a checkerboard/colored quad first, before imported images.

Treat cross-process input pass-through as an experimental gate. `WS_EX_TRANSPARENT` by itself describes paint-order behavior; `HTTRANSPARENT` is not a universal cross-thread click-through guarantee. Prove the selected HWND/compositor style combination with real mouse, wheel and pen events reaching CSP. If necessary, isolate a layered-window presentation fallback using the same D3D-rendered output; measure the readback/upload cost and rerun the exclusion test. Do not silently mix incompatible layered-window and composition paths.

Decode WIC images into a documented format; bound dimensions and allocation arithmetic (initial limit: 16,384 per dimension and 256 MiB decoded, with checked multiplication). Respect orientation or explicitly normalize it. Premultiply once, not twice. Provide fit, reset placement, opacity and an asymmetric test asset. Keep the image texture immutable between imports; transforms update constants, not the source bitmap.

Use a separate ordinary presentation target for ReferenceWindow. Avoid private overlay mirrors in visible debug thumbnails during recording. SDR BGRA is the initial contract; HDR monitor behavior is outside the first acceptance gate and must be identified in diagnostics.

## 7. WGC capture proof

Use `IGraphicsCaptureItemInterop::CreateForWindow` for the selected HWND and check WGC support at runtime. Use an appropriate D3D11 BGRA-capable device wrapped for WinRT. Prefer `Direct3D11CaptureFramePool::CreateFreeThreaded`; do not assume a dispatcher exists on a worker.

Handle item closed, zero/stale content, content-size changes, resizing frame pool buffers, cancellation, and device removal. Feature-check optional session properties such as cursor capture; do not require suppression of the capture border to pass the MVP. Capture the CSP window, not the entire desktop. Verify that our separate overlay is absent from the actual captured CSP texture; do not simply assume it.

GPU textures are the main transport, but OpenCV CPU processing requires readback. Copy only bounded, optionally downsampled canvas/Navigator regions to staging resources; double-buffer and measure waits. Respect row pitch. Attach capture timestamps, not just processing timestamps. Debug preview must display actual captured frames with sequence/size/age and be disabled in the recording workflow.

## 8. OBS Display Capture behavior — early go/no-go gate

OBS Display Capture captures a monitor; it is distinct from our internal WGC capture of CSP. Do not substitute OBS Window Capture when evaluating this requirement.

Apply `WDA_EXCLUDEFROMCAPTURE` to our private top-level HWND before showing tracing content. Check the return value, immediately capture failure codes, and read back affinity. Reapply after HWND recreation. Leave the ordinary reference window at `WDA_NONE`. API success is necessary but not proof that the chosen OBS path omits the image. Windows documents limits to this mechanism and older systems do not provide identical behavior. [Microsoft API contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity).

Exact test procedure:

1. Record OS build, GPU/driver, SDR/HDR, monitor/DPI arrangement, OBS version, Display Capture method label and settings, CSP version, app commit and renderer path.
2. Use a local recording, not a live broadcast. Add OBS **Display Capture** for the monitor containing CSP. Move OBS preview to another monitor or minimize it to avoid recursive display capture. [OBS source description](https://obsproject.com/kb/display-capture-sources).
3. Show an unmistakable magenta asymmetric tracing marker over a CSP test drawing and a different green marker in ReferenceWindow. Confirm both are physically visible.
4. In a controlled test with nonsensitive assets only, briefly set private affinity to `WDA_NONE`: both markers must be recorded. This positive control verifies the source actually sees the intended screen region.
5. Restore exclusion; record at least 60 seconds while drawing, moving/resizing CSP, changing overlay opacity, toggling overlay visibility, and covering part of the canvas with the ordinary reference window.
6. Play back the saved recording and inspect still frames across transitions. Pass only if CSP is intact beneath the omitted overlay, no black/blank placeholder replaces it, the private marker never appears, and the ordinary reference remains visible. OBS preview alone is insufficient evidence.
7. Repeat for every available Display Capture method being considered, HWND recreation, monitor movement, and device recovery. Each configuration receives its own result. Do not claim untested configurations work.

If API affinity fails, keep private content hidden. If recording verification fails, mark the configuration unsupported and stop automatic-tracking investment until the display path is fixed. Manual acknowledgement of a passing test may enable normal use; the app cannot independently certify an OBS recording. This is a capture-behavior feature, not DRM or protection against a camera/external capture device.

## 9. Canonical transforms and calibration

Use homogeneous 3x3 matrices, **column vectors**, double precision on CPU. X increases right, Y down; positive displayed rotation is clockwise in that coordinate convention. Convert explicitly when uploading HLSL constants; do not mix row/column conventions.

Spaces: reference pixels `R`; document pixels or declared calibrated document units `D`; physical screen pixels `S`; overlay-local physical pixels `O`; capture pixels `C`.

```text
pO = M_SO * M_DS * M_RD * pR
M_DS = Translate(viewportAnchorScreen) * Rotate(theta) * Scale(zoom)
       * Flip(fx, fy) * Translate(-documentAnchor)
M_SO = Translate(-overlayPhysicalScreenOrigin)
```

`M_RD` is deliberate reference placement and changes only during alignment. `M_DS` is current canvas state. Capture measurements are converted through a separately calibrated `M_CS` (capture to screen) before fusion. Clip in overlay-local viewport coordinates. Window movement changes the screen mapping, not document placement. DPI is not an extra arbitrary scale on already-physical coordinates.

For initial calibration, the user selects the canvas ROI, declares document dimensions if known, and aligns the reference with explicit position/scale/angle/flip controls. When dimensions or absolute zoom are unknown, define a named local document frame at calibration; relative tracking can still work, but never label its scale as verified CSP percent. Save a baseline CSP keyframe and corresponding canonical matrix.

Track relative-to-keyframe transforms and periodically relocalize to validated anchors. A canonical matrix does **not** eliminate CV drift by itself. Avoid endless frame-to-frame multiplication without an anchor. Test noncommuting transform order, rotation/scale about off-center pivots, negative monitor origins, inverses, reflections, and screen/capture conversion. The equivalent representations of two flips plus 180-degree rotation need a deterministic convention; do not pretend pixel observations uniquely identify UI flip flags.

## 10. Hybrid CSP synchronization

No validated public viewport API is assumed. The following is a prototype hypothesis to test against the installed CSP configuration.

**A. Canvas visual measurement (first source).** Start with feature detection/matching or optical flow across captured canvas ROIs; reject cursor, UI, borders, and changing stroke regions where possible. Robustly fit translation + uniform scale + rotation with RANSAC. Score inlier count, spatial coverage, residual and plausible step size. Reject shear/nonuniform scale rather than absorbing UI mistakes. Compare against keyframes as well as the previous frame. Temporal smoothing must not conceal gross errors.

**B. Navigator measurement (second source).** User chooses Navigator ROI and calibrates its thumbnail-to-document mapping. Test whether the viewport indicator/thumbnail provides usable position, scale, rotation or parity evidence on this CSP version. Do not assume it always does: themes, tiny thumbnails, indicator shape and canvas updates can defeat it. Report only measured components with confidence. Initially use image geometry/template detection; OCR is a separate future decision with its own dependency, not silently bundled with OpenCV. UI Automation may provide values only if an inspection proves them accessible.

**C. Input prediction (optional).** Raw Input and a configurable observed gesture mapping can predict short deltas while awaiting visual frames. Gate to the selected foreground target and never swallow/inject input. Shortcuts and wheel behavior are configurable, pen/touch may differ, and input does not prove CSP applied a transform. Prediction expires after at most 100 ms without confirmation; discard on contradictory visual evidence. WinEvent primarily supplies window geometry/lifecycle, not canvas navigation.

**Flip handling.** A rotation/positive-scale partial affine estimator cannot discover reflection. Evaluate explicit mirrored source hypotheses (including coordinate remapping), then fit the orientation-preserving transform within each hypothesis. Require asymmetric evidence and hysteresis before changing parity. A blank or symmetric canvas cannot reliably distinguish flips. When ambiguous, pause or require explicit user confirmation; never silently continue with a guessed reflection.

**Fusion.** Convert observations to a common timestamp/space, reject wrong generations, gate by residual/age, and publish one immutable absolute transform. Measured visual/Navigator anchors correct input predictions. Do not average raw matrices across parity changes. Fuse translation, log-scale and unwrapped angle within a fixed parity hypothesis; start with a transparent weighted/gated model before considering a Kalman filter.

**State machine.** `Unattached -> Calibrating -> Tracking -> Degraded -> Lost`, with `Paused` reachable by user/system policy and `Unavailable` for capture/device faults. Initial proposed defaults: stale measurement at 150 ms enters Degraded; render only if predicted quality still passes; at 300 ms without trustworthy evidence enter Lost and hide. Show state and resync control in the ordinary panel. Resume only after several mutually consistent measurements or explicit recalibration. Tune using recorded data, documenting changes.

Document switches, major layout changes, unknown ROI mapping, target replacement, and device loss invalidate calibration. If reliable document-switch detection is unavailable, require an explicit stop/recalibrate workflow for every switch and label that limitation. Do not reuse a confident old transform on a different document.

## 11. Milestones and phased implementation order

Each gate requires a build, automated tests available at that stage, and the specified manual evidence. Never skip a failed gate by marking it complete.

1. **M0 — reproducible shell:** repository, dependency baseline, debug/release presets, Win32 control window, DPI manifest and one real core test. Gate: clean configure/build/test from a fresh directory.
2. **M1 — detection:** enumerate/select real CSP; geometry and lifecycle display. Gate: two candidates disambiguated, close/minimize/restore handled, mixed-DPI coordinates verified.
3. **M2 — transparent surface:** D3D/compositor and private marker; affinity; real cross-process pass-through. Gate: no focus theft, mouse/wheel/pen drawing works, transparent background, safe emergency hide.
4. **M3 — capture:** actual WGC CSP preview and lifecycle. Gate: continuous updated pixels, resize/close cleanly handled, no overlay feedback in CSP capture.
5. **M4 — reference and OBS proof:** WIC image, ordinary reference window, opacity and independent affinity. Gate: saved Display Capture recording passes section 8. Stop here if exclusion cannot be demonstrated.
6. **M5 — manual transform engine:** calibrated spaces, reference placement, explicit pan/zoom/rotation/flip. Gate: numerical tests and asymmetric visual alignment pass.
7. **M6 — visual tracking:** bounded ROI readback, replay fixtures, visual tracking and quality states, then reflection hypotheses. Gate: known synthetic transforms and real textured CSP navigation match within targets; low-information scenes fail safely.
8. **M7 — hybrid synchronization:** Navigator measurements, optional input/UIA, timestamp fusion and relocalization. Gate: evidence improves recovery without increasing confident errors; disabled observers do not break baseline visual/manual modes.
9. **M8 — integration:** fault recovery, performance measurement, mixed-DPI tests, long run, clean build and final OBS regression. Gate: all must-pass cases have recorded evidence and limitations are documented.

## 12. Acceptance criteria and test plan

Functional must-pass:

- User can select actual CSP, import PNG/JPEG, adjust opacity and align; visible image stays within the selected viewport.
- Mouse, wheel and representative pen pressure/strokes reach CSP without focus change in tracing mode. Alignment mode is deliberately interactive and reversible.
- Local screen has private overlay and ordinary reference; saved OBS Display Capture recording has CSP and ordinary reference only, including transitions and recreation.
- Invalid capture/target/geometry hides private content; lost tracking hides rather than leaving a misleading stationary reference.
- Textured test documents support pan/zoom/rotation and explicit parity tests; blank/symmetric scenes report insufficient evidence.
- Close/reopen CSP, cancel import, corrupt images, resize, monitor changes and recovery do not crash or leave an orphan overlay.

Provisional measurable targets (not achieved claims): 60 Hz rendering where the display supports it; CV processing initially 15–30 Hz; observation-to-present age p95 <=100 ms; settled registration error median <=2 physical pixels and p95 <=5 across at least 20 distributed known landmarks at 1080p/1440p; rotation <=0.5 degrees and relative scale <=0.5% when ground truth exists; recovery <=1 second after usable textured content returns. Record hardware, workload, sample count and failures. Real CSP video without ground truth supports only manually annotated landmark/error claims. Use a synthetic Win32 canvas fixture with known transforms for exact metrics.

Performance characterization: 30-minute drawing/navigation session, rolling CPU/GPU usage, frame age/drop count, staging stall duration, working set/VRAM trend. Initial budget goal: <=1 GiB process working set and no sustained growth after warm-up for one 4K reference plus 1440p capture. CPU/GPU budgets are hardware-dependent: record measured baselines before assigning numeric release limits. Report p50/p95/max, not only FPS. Never label queue age as measured end-to-end physical display latency.

Automated layers:

- Pure core tests: matrices, inverses, calibration, transform order, rotations/reflections, finite checks, state timeout boundaries, generation invalidation and timestamp ordering.
- Image tests: known dimensions/alpha/orientation, unsupported/corrupt files, overflow guards and allocation limits.
- Tracking tests: generated pan/zoom/rotation/reflection, outliers/stroke additions, blur, symmetric/blank scenes, parity ambiguity, stale observations and relocalization. Test against known ground truth rather than duplicating implementation formulas.
- Replay: seeded synthetic sequences with metadata; optional user-approved local CSP recordings. Do not commit private artwork. Assert error bounds, confidence transitions and no confident wrong recovery.
- Integration fixture: a separate-process Win32 canvas test application provides known geometry, transforms and input counters. It does not replace testing real CSP/OBS/pen behavior.
- Hardware/manual: Display Capture, mixed-DPI and negative-origin monitors, focus/modal interactions, pen, GPU debug layer and resize/device-loss paths. CI cannot certify these.

Put results and artifact paths in VALIDATION.md. Red/blocked/not-run are valid report states, never equivalent to passed.

## 13. Coding standards and development discipline

Small steps: at most five implementation/config/test files per Cursor prompt, plus one validation/progress log update. If more are required, split before editing. Keep the application buildable after each step; no speculative interfaces for future modules. Do not replace working code with a scaffold during later prompts.

Use RAII for COM, handles, event registrations and threads; `winrt::com_ptr`/appropriate smart ownership; explicit HRESULT/Win32 error conversion. Capture `GetLastError` immediately. No uncaught exceptions across Win32/WinRT callbacks. No manual ownership hidden in raw pointers; HWND values are borrowed identities with checked lifecycle.

Use explicit units/types, `std::chrono` timestamps, checked sizes and finite transform validation. Keep third-party/Windows headers out of the pure core. Prefer narrow interfaces and immutable snapshots. Document threading and GPU ownership next to interfaces. Avoid detached workers, unbounded queues, busy loops, global mutable singletons and indefinite UI-thread waits.

Enable `/W4`, `/permissive-`, Unicode, consistent C++23 mode and first-party warnings-as-errors once clean; do not apply that policy to dependency sources. Formatting configuration should be small and consistent. Log errors with stage, HRESULT/Win32 code and session generation; do not include artwork, full private paths or raw input streams by default.

Use standard-user execution; report elevation mismatches rather than recommending admin rights as the default fix. Do not download unpinned executable tools or change global security settings. No automatic pushes, releases or installers. End each implementation step with changed files, actual build/test results, manual checks, known limits and the next unblocked prompt number.

## 14. Proposed repository layout

```text
TracingApp/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json
  .gitignore
  .clang-format
  project-docs/
    README.md
    MASTER.md
    CURSOR_PROMPTS.md
    VALIDATION.md
  src/
    app/             # main, controls, session orchestration
    core/            # dependency-free transforms and policies
    platform/        # target, geometry, affinity, observers
    graphics/        # device, compositor, renderer
    capture/         # WGC and frame lifecycle
    image/           # WIC decoding
    tracking/        # visual, Navigator, fusion
    diagnostics/     # metrics and event recording
  shaders/           # textured quad and transform constants
  tests/
    unit/
    replay/
    fixtures/        # generated/synthetic assets only
    harness/         # known-transform Win32 test canvas
  resources/         # manifest and non-private assets
  scripts/           # focused verification helpers if needed
  out/               # ignored builds, logs, local test evidence
```

Create files when used; the tree is not an instruction to generate empty modules. Keep mutable runtime files under a user data directory or ignored `out/` during development. Treat synced `sources/` as read-only.

## 15. Risks and decisions

- **Exclusion varies with capture configuration:** early saved-video test; maintain a tested configuration list; do not promise all OBS backends.
- **Composition and pass-through interaction:** prototype before importing images; test real cross-process pen behavior; keep an isolated measured fallback.
- **No direct canvas transform API:** hybrid observer, calibration and confidence; manual mode remains useful if automation proves unreliable.
- **Canvas changes while drawing:** outlier rejection, spatial coverage checks, bounded keyframe refresh; avoid replacing anchors during low confidence.
- **Blank/symmetric art and flips:** observability limit; pause/resync, never fabricated certainty.
- **Navigator or accessibility changes:** version/theme-specific adapters and optional capabilities; no brittle hardcoded pixel coordinates as a universal solution.
- **DPI/capture/client mismatch:** typed spaces, generation matching and explicit fixture tests.
- **GPU readback stalls:** bounded ROI, downsampling, staging ring and telemetry before optimization.
- **Focus, floating panels, document changes:** conservative visibility policy and explicit recalibration where detection is not reliable.
- **HDR/device changes:** start SDR, identify unsupported modes, hide/rebuild on device removal.
- **Toolchain drift:** pin dependency baseline, record compiler/SDK/CMake versions, reproduce fresh-directory build.

The prototype succeeds first by proving overlay/capture/recording behavior, then by honestly measuring tracking reliability. Failure to obtain reliable tracking on a scene is acceptable only if reported safely; displaying a confidently wrong alignment is not.

## 16. Primary implementation references

These document APIs, not a claim that TracingApp has been tested. Checked during planning on 2026-09-25.

- [Capture item from HWND](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow): Win32 interop entry point.
- [Microsoft WGC sample](https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/tree/master/cpp/ScreenCaptureforHWND): lifecycle and device interop reference; adapt ownership deliberately.
- [Extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles): consult actual style semantics when proving pass-through.
- [CMake VS 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html): generator availability and architecture selection.
- [OpenCV robust transform estimation](https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html): affine-estimation API reference; reflection handling remains a separate design task.

Earlier discussion supplied the product intent, particularly the two-window recording behavior. This document supersedes its WinUI-first suggestion and unverified dependency-version assumptions.
