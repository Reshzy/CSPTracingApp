# TracingApp — validation record

Planning baseline: 2026-09-25. Prompt 01 Debug bootstrap compiled and tested on 2026-09-25. Prompt 02 pinned vcpkg/GoogleTest and verified Debug+Release on 2026-09-25. Prompt 03 added target discovery with automated selection tests; real CSP window selection remains incomplete. Prompt 04 embedded PerMonitorV2 DPI awareness and target geometry/lifecycle reporting; real CSP mixed-DPI/lifecycle remains NOT RUN. Prompt 05 created a BGRA D3D11 device with RAII immediate-context ownership and GPU-free lifecycle tests. Prompt 06 added a DirectComposition overlay HWND, WDA_EXCLUDEFROMCAPTURE before show, emergency hide, and GPU-free visibility tests. Prompt 07 added tracing/alignment modes and a separate-process InputProbe; the DirectComposition HWND/style combination failed cross-process mouse/wheel pass-through. Prompt 07-fallback replaced that path with an isolated WS_EX_LAYERED UpdateLayeredWindow presenter; InputProbe click/wheel pass-through now PASSes on both monitors, and tracing hits the titled CSP HWND without overlay activation. Prompt 08 added a C++/WinRT WGC session with a free-threaded frame pool, owned-frame handoff, and control-window counters/metadata. Prompt 09 added an opt-in ordinary WDA_NONE capture preview, frame-pool Recreate on content-size change, measured capture-to-client mapping, and overlay hide on capture loss. Prompt 10 added Unicode IFileOpenDialog plus WIC PNG/JPEG decode into an owned 32bppPBGRA buffer with 16,384/axis and 256-MiB limits. Prompt 11 uploads that buffer once to an immutable D3D11 texture, compiles shaders/Image.hlsl with discovered SDK fxc, and draws a premultiplied textured quad onto the existing layered overlay HWND with opacity/fit/reset. Prompt 12 adds a separate ordinary `WDA_NONE` ReferenceWindow with its own DXGI swap chain that reuses that immutable texture; overlay exclusion stays `WDA_EXCLUDEFROMCAPTURE`. Prompt 13 added labelled temporary OBS diagnostics (WDA_NONE positive-control latch, skip overlay image present, Recreate overlay HWND) and ran local OBS Display Capture recordings. Saved playback showed the ordinary green ReferenceWindow and intact CSP, but never the magenta `UpdateLayeredWindow` overlay, including while affinity readback was `WDA_NONE`. The 13-present repair replaced ULW with `CreateSwapChainForHwnd` (`presentPath=dxgi-hwnd`, no `WS_EX_NOREDIRECTIONBITMAP`, no DirectComposition). Premultiplied alpha swap-chain create failed `DXGI_ERROR_INVALID_CALL` (`0x887A0001`); ignore-alpha plus `DwmExtendFrameIntoClientArea` succeeded. CAPTUREBLT/PrintWindow and saved Automatic Display Capture now show the magenta/yellow L. WDA_NONE positive control PASSed and restored `WDA_EXCLUDEFROMCAPTURE` omitted the overlay with no black hole on Automatic. DXGI Desktop Duplication and Windows 10 (1903+) methods were not re-recorded this repair. Overlay clear is opaque black (ALPHA_IGNORE). Pen/pressure remains NOT RUN. Prompt 14 added a Windows-free double-precision Transform2D engine (column-vector, X-right/Y-down, spaces R/D/S/O/C) with GoogleTest coverage and a canned control-window diagnostic; actual pO=(2040,102) matched the hand-computed expected point. Prompt 15 substep A added a Windows-free Calibration engine (ROI validation, M_CS, M_RD vs M_DS, overlay-local clip, invalidation, local vs declared-document unit labels) with GoogleTest coverage, control-window ROI/align/canvas controls, and overlay HWND clipped to the applied canvas ROI. Prompt 15 substep B uploads composed M_RO as an overlay-local 2x3 affine into Image.hlsl, enables RS scissor on the ROI-sized overlay RT, and keeps ReferenceWindow on axis-aligned FitPlacement. Automatic tracking remains disabled. CSP titled-window ROI HWND clip and same-DPI window-move (M_RD stable) PASS; CopyFromScreen cannot sample WDA_EXCLUDEFROMCAPTURE so eyeballed landmark pixels remain NOT RUN. Prompt 16 substep A added checked canvas/Navigator ROI clip, optional integer downsample, RowPitch pack, and a two-slot D3D11 staging ring on the WGC owned copy; CPU buffers keep capture timestamps/sequence/generations. Prompt 16 substep B maps applied client-relative canvas ROI through measured capture-to-client offset into that ring (`roiSrc=applied-mapped`); idle CSP may deliver sparse WGC frames so the mapped clip appears on the next owned copy. Live titled CSP: clipped=80,80 640x480 ds=1, copyUs p50=3µs, mapWaitUs p50=0 max=1, pending=1, hasCpu=yes, zeroCopy=no. Resize invalidated calibration (`client-size-changed`) and fell back to full-content with pending still 1. Prompt 17 added opencv4 4.12.0#7 (calib3d/intrinsics/thread only) and a canvas-to-canvas ORB + RANSAC similarity estimator; synthetic GoogleTests PASS, including reflection-unsupported rejects. Prompt 18a added TrackingSession (Unattached/Calibrating/Tracking/Degraded/Lost/Paused/Unavailable, provisional 150/300 ms age, keyframe M_DO snapshots, worker unlock-then-join) with 11 GoogleTests PASS; VisualTracker is now linked into TracingApp. Prompt 18b exposes CaptureSession LastRoiBuffer, feeds TrackingSession on each owned WGC copy, and applies snapshot M_DS while attached; worker GoogleTests PASS. Live titled CSP: ROI CPU feed and hide-on-Lost PASS; settled Tracking on the default 80,80 640x480 ROI did not hold (poor-coverage / observation age). Prompt 19a recovers FlipX/FlipY via remapped orientation-preserving hypotheses, canonicalizes FlipX+180 to FlipY (and FlipX*FlipY to 180° with no flip flags), pauses on ParityAmbiguous, and keeps the origin keyframe; 28 VisualTracker GoogleTests PASS. Prompt 19b adds session-level parity hysteresis (2 consecutive opposite-parity Ok estimates), Pause-on-ParityAmbiguous via `SubmitEstimateForTest`, freeze while Paused, optional second validated anchor with origin-keyframe residual as `driftRms` vs cumulative `reacquire` events, and 19 TrackingSession GoogleTests PASS. Live CSP flip/parity remains NOT RUN. MASTER pixel targets remain unclaimed. Prompt 20a added a GPU-free `NavigatorObserver` (OpenCV image-geometry viewport-indicator, measured-only translation/relative-scale/rotation, no OCR, no parity from a rectangle) with 14 GoogleTests (1 skipped local-opt-in fixture) and control-window Apply Nav ROI / Disable Nav that do not fuse into TrackingSession. Prompt 20b adds a second CaptureSession Navigator-kind `RoiReadback` (no full-content fallback), maps client-relative Nav ROI through measured capture-to-client offset, and feeds packed BGRA into `Observe()`; live CSP Navigator pan/zoom/hidden/themes remain NOT RUN (no CLIPStudioPaint process this step). Timestamp fusion remains Prompt 23.

Use PASS / FAIL / BLOCKED / NOT RUN. Each entry must include actual evidence. A successful API call or synthetic replay does not certify OBS behavior or real CSP tracking.

## Environment — fill when implementing

- App commit/build and renderer path: HEAD `2f21f56` (`feature/init`; Prompt 20b files uncommitted). Win32 control window plus WS_EX_LAYERED overlay HWND presented with `CreateSwapChainForHwnd` (`presentPath=dxgi-hwnd`, default WDA_EXCLUDEFROMCAPTURE; temporary WDA_NONE positive-control latch; `SetLayeredWindowAttributes(LWA_ALPHA)`; no `WS_EX_NOREDIRECTIONBITMAP`; no ULW; no DirectComposition) and a separate ordinary WS_OVERLAPPEDWINDOW ReferenceWindow (WDA_NONE, DXGI swap chain); skip-image-present diagnostic keeps the magenta/yellow test marker on the overlay swap-chain RTV; ImageRenderer draws into that same RTV using overlay-local 2x3 affine constants plus RS scissor; after each owned WGC copy, canvas `RoiReadback` packs BGRA CPU rows (not zero-copy) with `roiSrc=applied-mapped` or full-content fallback; a second Navigator-kind `RoiReadback` copies only an applied-mapped Navigator ROI (`navRoiSrc=none` until Apply Nav ROI maps through measured capture-to-client offset; no full-content fallback); `TrackingSession` owns a worker and publishes immutable `TransformSnapshot`s with `parity`/`driftRms`/`reacquire`/`secondary` fields; origin keyframe is kept for the session and an optional second validated anchor is used only to relocalize when origin residual is worse; `VisualTracker` evaluates None/FlipX/FlipY remapped hypotheses then an orientation-preserving fit; `CaptureSession::LastRoiBuffer` still feeds canvas tracking; `LastNavigatorRoiBuffer` packed BGRA is bound as `NavigatorPixelFormat::Bgra32` into `NavigatorObserver::Observe` after PumpHandoff without Detach. `NavigatorObserver` remains independent (no OCR, no fusion); status prints `FormatNavigatorObservation` plus `navBuffer=yes|no`. shaders compiled with discovered `fxc.exe` `10.0.26100.8249` at `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe`; capture preview stayed off; Debug `out/build/windows-debug/Debug/TracingApp.exe` (2620928 bytes, 2026-09-26 09:57:51), `TracingApp.NavigatorTests.exe` (1131008 bytes, 2026-09-26 09:57:59), `TracingApp.BootstrapTests.exe` (112640 bytes, 2026-09-26 09:57:53); Release not rebuilt this step
- Windows edition/build and SDR/HDR state: registry `ProductName` Windows 10 Pro, `DisplayVersion` 25H2, build 26200.9457 (`EditionID` Professional). dxdiag 2026-09-25: both NVIDIA outputs HDR Support=Supported and AdvancedColorSupported, but Display Color Space=`DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709` (SDR gamma 2.2, not HDR active). OBS profile ColorSpace=709, ColorFormat=NV12, SdrWhiteLevel=300.
- Visual Studio/MSVC, Windows SDK, CMake, vcpkg baseline/triplet: Visual Studio Community 2026 18.10.2 at `C:\Program Files\Microsoft Visual Studio\18\Community`; MSVC 14.51.36231 / `cl` 19.51.36260.0 (`Hostx64/x64`); Windows SDK 10.0.26100.0; CMake/CTest 4.3.1-msvc1 (not on PATH) at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`; generator Visual Studio 18 2026 x64; vcpkg VS 2026 bundled at `%VCPKG_ROOT%` = `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg` (not a git working copy); `vcpkg-bundle.json` `embeddedsha` / `vcpkg.json` `builtin-baseline` `1460b31b08c42cc2e9ac2c79f45ec8707e2675e2`; `vcpkg.exe version` `2026-07-27-98d7cb0cf1f4686a3e43aa5672b6230c1d56bce8`; triplet `x64-windows`
- OpenCV and GoogleTest resolved versions: `opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7` from git registry (`microsoft/vcpkg@023accb4d65ee6957a61c2795030ca725517fbb2`); `find_package` reported OpenCV 4.12.0 (components core imgproc features2d calib3d; flann DLL copied as a features2d dependency). Default opencv4 features (dnn/highgui/codecs/dshow/msmf/win32ui/gapi/directml) were disabled. GoogleTest `gtest:x64-windows@1.17.0#3` from git registry (`microsoft/vcpkg@dab84cf3bb50ef2ca3e0b0212c1d55e9e05a75bc`)
- C++/WinRT: SDK `10.0.26100.0` headers; TracingApp links `WindowsApp.lib` and uses `IGraphicsCaptureItemInterop::CreateForWindow` plus `Direct3D11CaptureFramePool::CreateFreeThreaded`. `cppwinrt.exe` v2.0.250303.5. Windows App SDK not adopted.
- GPU, driver, monitor sizes/refresh/DPI/origins: NVIDIA GeForce RTX 3060 driver 32.0.16.1656; AMD Radeon(TM) Graphics driver 32.0.21042.62. Prompt 13 DXGI adapter from app status: `NVIDIA GeForce RTX 3060` vendor=0x10DE device=0x24C7, D3D_FEATURE_LEVEL_11_1. Screens: `\\.\DISPLAY2` primary Bounds={X=0,Y=0,Width=1920,Height=1080} effective DPI 96x96, EG24XT 1920x1080 32-bit 240Hz; `\\.\DISPLAY1` Bounds={X=1920,Y=0,Width=2560,Height=1440} effective DPI 96x96, R27qe 2560x1440 32-bit 180Hz. Negative-origin monitor not present. Mixed-DPI not present (both 96).
- CSP version/locale/theme/workspace and drawing device: CLIPStudioPaint.exe 5.0.0 (`FileVersion` 5.0.0.0) at `C:\Program Files\CELSYS\CLIP STUDIO 1.5\CLIP STUDIO PAINT\CLIPStudioPaint.exe` pid 36888 during Prompt 06; CLIPStudio.exe launcher pid 20700. Locale/theme/workspace/pen NOT RECORDED
- OBS version, Display Capture method/settings, recording resolution/FPS: OBS Studio 32.2.2 (`C:\Program Files\obs-studio\bin\64bit\obs64.exe`). Profile Untitled Advanced: RecFilePath `I:/Videos/Obs/GD`, RecFormat2=mkv then AutoRemux mp4, RecEncoder=`obs_nvenc_hevc_tex`, Base/Output 1920x1080, FPSCommon=60, ColorSpace=709. Display Capture source `Main Monitor` of `R27qe: 2560x1440 @ 1920,0`, Capture Cursor on, Force SDR off. Prompt 13 methods: Automatic, DXGI Desktop Duplication, Windows 10 (1903 and up). 13-present retest: Automatic only (OBS left on Automatic). OBS window stayed on primary DISPLAY2. Capture preview in TracingApp stayed off.

## Milestone status

- M0 reproducible shell/build/tests: PASS (Prompts 01–02 plus Prompt 04 DPI manifest: local Git, Win32 control window, PerMonitorV2 RT_MANIFEST, vcpkg baseline, first-party `/W4` `/permissive-`, GoogleTest, Debug configure/build/test)
- M1 CSP discovery/geometry/lifecycle: NOT RUN (Prompt 03 automated tests PASS; Prompt 06 enumerated live CLIPStudioPaint.exe and selected one PAINT row; mixed-DPI move, minimize/restore/close, and titled main-window vs panel disambiguation remain incomplete)
- M2 transparency/input/affinity: PASS for mouse/wheel pass-through on DISPLAY1/R27qe after 13-present (dxgi-hwnd). Prompt 05 device PASS; Prompt 06 affinity/emergency hide/visibility policy PASS; Prompt 07 DComp path FAIL (superseded). Prompt 07-fallback ULW pass-through on both monitors remains historical PASS. 13-present keeps `WS_EX_LAYERED` + tracing `WS_EX_TRANSPARENT` without `NOREDIRECTIONBITMAP`; DISPLAY1 InputProbe click/wheel PASS without overlay activation; DISPLAY2 click landed on `MozillaDialogClass` (setup overlap, overlay did not activate); alignment intercepts overlay HWND; emergency hide PASS. Visual clear is opaque black (`DXGI_ALPHA_MODE_IGNORE`). Pen/pressure NOT RUN. Titled CSP HWND hit-test not re-run this repair.
- M3 actual CSP capture/resize/no-feedback: PASS for titled CLIP STUDIO PAINT WGC preview + pool Recreate + marker-absent-from-capture-texture (Prompt 09). Prompt 08 counters/content-size remain PASS. Item-closed (CSP close while capturing) NOT RUN.
- M4 imported image/ordinary reference/OBS playback: PASS for OBS Display Capture **Automatic** on the dxgi-hwnd overlay (13-present). WIC/import/renderer/ReferenceWindow remain PASS (Prompts 10–12). WDA_NONE saved playback contains the magenta/yellow L (2016 magenta / 1378 yellow samples in a 1920x1080 still). Restored exclude playback omits the marker with InputProbe/CSP chrome intact and no black overlay hole. DXGI Desktop Duplication and Windows 10 (1903+) methods NOT RUN this repair (ULW-era FAIL records remain). Ordinary green ReferenceWindow was not shown in the 13-present clips (Prompt 12/13 PASS still stands). Opaque black overlay body is a remaining tracing-transparency defect, not an exclusion failure.
- M5 manual transforms/calibration: PARTIAL (Prompt 14 numerical engine PASS; Prompt 15a ROI/mapping/invalidation tests PASS; Prompt 15b ImageRenderer affine + RS scissor PASS, titled CSP ROI HWND clip PASS, same-DPI window-move M_RD-stable PASS; eyeballed asymmetric landmark pixels NOT RUN because CopyFromScreen omits WDA_EXCLUDEFROMCAPTURE; mixed-DPI still unavailable)
- M6 visual/parity tracking and confidence safety: PARTIAL (Prompt 16a/16b ROI readback PASS; Prompt 17 synthetic ORB+RANSAC estimator PASS with measured textured errors below 0.2 px / 0.05 deg on 320x240 fixtures; Prompt 18a TrackingSession GoogleTests PASS; Prompt 18b worker-path textured translation / blank GoogleTests PASS and live titled-CSP ROI feed + hide-on-Lost PASS; Prompt 19a synthetic FlipX/FlipY / combined / two-flip≡180 / symmetric-ambiguous / prolonged-outlier / return-to-keyframe GoogleTests PASS; Prompt 19b session hysteresis / Pause-on-ParityAmbiguous / return-to-keyframe worker / secondary-anchor relocalization / separate drift vs reacquire GoogleTests PASS. Settled live Tracking on default 80,80 640x480 ROI did not hold in 18b; live CSP flips NOT RUN.)
- M7 hybrid observers/fusion: PARTIAL (Prompt 20a synthetic NavigatorObserver GoogleTests PASS; Prompt 20b CaptureSession Navigator ROI feed + packed-BGRA Observe GoogleTests PASS; control smoke navBuffer=no navRoiSrc=none; live CSP Navigator appearance/pan/zoom/hidden/themes NOT RUN; timestamp fusion remaining Prompt 23)
- M8 integration/performance/final recording regression: NOT RUN

## Manual checklist

- [ ] Actual CSP painting window selected; ambiguous instances require selection.
- [ ] Physical coordinates correct at 100%, 125%, 150%, 200% scale where available.
- [ ] Negative-origin and mixed-DPI monitor movement tested or explicitly not available.
- [ ] Overlay transparent outside image and clipped to selected canvas. (15b: titled CLIP STUDIO PAINT client (1920,0) 2576x1440; applied ROI 80,80 640x480 placed overlay at (2000,80) 640x480 with clipO=(0,0,640,480) and RS scissor=overlay-rt. ALPHA_IGNORE body is still opaque black inside that HWND. CopyFromScreen of the overlay rect showed CSP chrome, not the private texture.)
- [ ] Mouse, wheel and pen/pressure reach CSP; overlay does not take focus. (13-present: DISPLAY1 InputProbe click/wheel PASS, overlay not foreground; DISPLAY2 click hit MozillaDialogClass; CSP titled-HWND and pen NOT RUN)
- [x] Interactive alignment toggles back to tracing without stuck input. (13-present: WindowFromPoint returns TracingAppOverlayWindow in alignment on DISPLAY1 and DISPLAY2)
- [x] Emergency hide works; minimize/target close/unrelated focus hide appropriately. (13-present: Emergency Hide hides overlay HWND. Minimize/target-close/unrelated-focus not re-run this repair)
- [ ] Modal dialogs/floating palettes do not have misleading tracing content above them.
- [ ] Captured CSP pixels update; resize/stale/closed capture handled.
- [ ] Capture/client offsets calibrated; overlay absent from actual CSP capture.
- [x] PNG/JPEG/Unicode path/corrupt/oversized images handled.
- [x] Ordinary reference HWND remains WDA_NONE and independent of tracking.
- [x] OBS Display Capture positive control records the private magenta marker (Automatic, 13-present dxgi-hwnd, WDA_NONE). Green ordinary reference NOT RUN in these clips (Prompt 13 still has green PASS). DXGI DDA and Windows 10 (1903+) positive-control retests NOT RUN.
- [x] Exclusion test playback omits private marker with intact underlying content (Automatic, 13-present: InputProbe/CSP chrome visible, magenta=0, no black overlay hole). Hide/show/recreate/opacity/CSP-resize and non-Automatic methods NOT RUN this repair.
- [x] Ordinary reference visible in playback throughout its visible intervals. (green TracingApp Reference on R27qe Display Capture; Prompt 13, not re-shown in 13-present clips)
- [x] No private-frame flash/black rectangle during show/hide/resize/recreation. (Prompt 13 exclude t38/t50; 13-present exclude crop shows InputProbe not a black hole. Recreate/opacity/resize NOT RUN this repair)
- [ ] Each intended OBS method and monitor configuration tested separately. (13-present Automatic PASS on R27qe 2560x1440; DXGI Desktop Duplication and Windows 10 (1903+) NOT RUN after the presenter change; mixed-DPI/negative-origin still unavailable)
- [ ] Manual pan/zoom/rotation/flips obey known asymmetric landmarks. (Prompt 14 canned pO=(2040,102) PASS; Prompt 15a GoogleTest conversions/clip/invalidation PASS; Prompt 15b imported 96x48 asymmetric PNG onto titled CSP; after M_RD R+/FX and M_DS R+/FY, status affine=[[-6.66667,0,20.71],[0,-6.66667,-77.27]] axisAlignedPlacement=no. Combined flips produced diagonal negative scale. Same-DPI move +80,+40: overlay followed, M_RD unchanged. Eyeballed L vs canvas pixels NOT RUN.)
- [ ] Visual tracking tested with textured drawing and new strokes. (17: synthetic textured pan/zoom/rotate and stroke-outlier fixtures PASS; 18b worker GoogleTest translation ~8,5 px PASS. Live titled CLIP STUDIO PAINT pid 36888: ROI feed enqueue PASS; one run keyframe inliers=683 conf=0.333 then Lost hide; another keyframe then poor-coverage Lost. Overlay did not stay locked through pan/zoom. MASTER 2 px not claimed.)
- [x] Bounded ROI readback copy/map wait and pending<=2 on CSP resize. (16b: titled CLIP STUDIO PAINT pid 36888; applied ROI 80,80 640x480 -> clipped=80,80 640x480 ds=1 roiSrc=applied-mapped copyUs p50=3us mapWaitUs p50=0 max=1 pending=1 hasCpu=yes zeroCopy=no; after +160x+100 resize invalidation=client-size-changed roiSrc=full-content-fallback pending=1 recreates=3. Re-apply set roiApplied=yes; next mapped copy not observed within 800 ms idle.)
- [ ] Blank/symmetric scenes and ambiguous flips degrade/hide safely. (17: synthetic blank/gradient reject `blank-or-low-texture`. 19a: left-right symmetric identity is `parity-ambiguous` conf=0. 19b: `SubmitEstimateForTest` ParityAmbiguous -> Paused, M_DS frozen until Resume/Resync; one FlipX rejected then second accepted; worker return-to-keyframe restores identity within 2.5 px; blank does not promote secondary. Live CSP blank canvas / CSP flips NOT RUN.)
- [ ] Navigator disappearance and optional observer disablement handled.
- [ ] Stale/wrong-generation observations cannot move overlay. (18a GoogleTest: seq 4 after 5 ignored; targetGeneration mismatch ignored; snapshot M_DS unchanged. 18b live: Lost hide=yes with overlay visibility=Hide reason=target-unusable after stale age; WGC continued.)
- [ ] Document/layout changes invalidate calibration or require explicit stop/resync.
- [ ] Target restart, device-recovery path and shutdown leave no orphan overlay.
- [ ] Debug previews disabled during recording; pixel recording opt-in only.
- [ ] Thirty-minute run records performance and memory trend.
- [ ] Fresh-directory Debug/Release builds and all discovered tests pass.

## Copy this record for each implementation step

```text
Step / milestone:
Date / commit:
Status:
Changed files:
Configure command + exit code:
Build command + exit code:
Test command + discovered/passed/failed counts:
Manual setup and exact actions:
Expected / actual:
Evidence paths (local, no private artwork committed):
Measured samples / p50 / p95 / max where applicable:
Known limitations / unavailable hardware:
Blocker or next prompt:
```

## Copy this record for each OBS configuration

```text
OS / GPU / driver / monitor / DPI / HDR:
OBS version / Display Capture method label / settings:
CSP version / app commit / renderer path:
Private HWND affinity set result / readback / error:
Positive control: PASS / FAIL / NOT RUN
Both markers physically visible: PASS / FAIL / NOT RUN
Saved recording playback: PASS / FAIL / NOT RUN
Private marker absent, including transitions: PASS / FAIL / NOT RUN
Underlying CSP intact (no black replacement): PASS / FAIL / NOT RUN
Ordinary reference visible: PASS / FAIL / NOT RUN
Recreated HWND / moved monitor retest:
Recording path / inspected timestamps:
Tested by / date:
Supported only for this configuration: YES / NO / UNVERIFIED
```

## Step records

```text
Step / milestone: 01 / M0 bootstrap (Debug)
Date / commit: 2026-09-25 / no commit (uncommitted working tree on empty main)
Status: PASS
Changed files:
  project-docs/README.md (moved)
  project-docs/MASTER.md (moved)
  project-docs/CURSOR_PROMPTS.md (moved)
  project-docs/VALIDATION.md (moved, then updated)
  .gitignore
  CMakeLists.txt
  CMakePresets.json
  src/app/main.cpp
  tests/unit/bootstrap_tests.cpp
  local git init only (no remote)
Configure command + exit code:
  "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --preset windows-debug
  exit 0
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe and TracingApp.BootstrapTests.exe written under out/build/windows-debug/Debug/
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 1, passed 1, failed 0 (TracingApp.BootstrapTests, 0.03s)
Manual setup and exact actions:
  Start-Process out/build/windows-debug/Debug/TracingApp.exe; wait 2s; CloseMainWindow; WaitForExit 5s
Expected / actual:
  expected visible ordinary control window titled TracingApp and clean exit 0
  actual PID 29852, MainWindowTitle=TracingApp, Responding=True, CloseMainWindow=True, ExitCode=0, no leftover process
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.BootstrapTests.exe
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  no DPI manifest, capture, OpenCV, or OBS
  Windows SDR/HDR, monitor refresh/DPI/origins not measured
Blocker or next prompt: 02 — Pin dependencies and compiler policy
```

```text
Step / milestone: 02 / M0 dependencies and compiler policy
Date / commit: 2026-09-25 / working tree on feature/init ahead of 93e2402 (uncommitted)
Status: PASS
Changed files:
  vcpkg.json (new)
  CMakePresets.json
  CMakeLists.txt
  tests/unit/bootstrap_tests.cpp
  .clang-format (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  first cmake --preset windows-debug on the Prompt 01 cache: exit 1
    find_package(GTest) failed (GTest_DIR-NOTFOUND); toolchain path was in cache but manifest install did not run
  cmake --preset windows-debug --fresh
  exit 0
    Running vcpkg install; fetched registry https://github.com/microsoft/vcpkg
    installed gtest:x64-windows@1.17.0#3, vcpkg-cmake:x64-windows@2024-04-23, vcpkg-cmake-config:x64-windows@2026-07-21
    SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
  cmake --preset windows-release --fresh
  exit 0
    restored the same three packages from local binary cache; binaryDir out/build/windows-release
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
    MSBuild 18.10.1-1.26427.6+3cd27c13e
    TracingApp.exe (54272 bytes) and TracingApp.BootstrapTests.exe (110592 bytes) under out/build/windows-debug/Debug/
  cmake --build --preset windows-release
  exit 0
    TracingApp.exe (11264 bytes) and TracingApp.BootstrapTests.exe (26112 bytes) under out/build/windows-release/Release/
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 1, passed 1, failed 0 (TracingApp.BootstrapTests, 0.04s)
  direct exe --gtest_list_tests: 5 ViewportPolicy cases; --gtest_brief=1: 5 tests, all PASSED
  ctest --preset windows-release --output-on-failure
  exit 0
  discovered 1, passed 1, failed 0 (TracingApp.BootstrapTests, 0.03s)
Manual setup and exact actions:
  confirmed VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake exists
  confirmed SDK cppwinrt\winrt\base.h and windows.graphics.capture.h exist; cppwinrt.exe printed v2.0.250303.5
  no CSP/OBS/pen checks in this step
Expected / actual:
  expected: manifest-mode GoogleTest, first-party /W4 /permissive-, Debug+Release configure/build/test, C++/WinRT headers present without Windows App SDK
  actual: matches; stale Prompt 01 cache required --fresh before vcpkg install ran
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.BootstrapTests.exe
  out/build/windows-release/Release/TracingApp.exe
  out/build/windows-release/Release/TracingApp.BootstrapTests.exe
  out/build/windows-debug/vcpkg-manifest-install.log
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  VCPKG_ROOT must be set in the environment; no personal path is committed
  VS bundled vcpkg is readonly / not a git checkout; baseline is the bundle embeddedsha
  vcpkg downloaded its own CMake 4.4.0 and 7-Zip 26.02 tools into the user vcpkg downloads cache during the first install
  DPI manifest, capture, OpenCV, overlay, and OBS remain later steps
  Windows SDR/HDR, monitor refresh/DPI/origins not measured
Blocker or next prompt: 03 — Target discovery
```

```text
Step / milestone: 03 / M1 target discovery (automated only)
Date / commit: 2026-09-25 / working tree on feature/init ahead of bd02be3 (uncommitted)
Status: PASS for automated tests; real CSP selection NOT RUN
Changed files:
  src/platform/TargetDiscovery.h (new)
  src/platform/TargetDiscovery.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  tests/unit/TargetSelectionTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 829952 bytes; TracingApp.TargetSelectionTests.exe 945152 bytes
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 2, passed 2, failed 0
    TracingApp.BootstrapTests 0.05s
    TracingApp.TargetSelectionTests 0.06s
  TargetSelectionTests --gtest_brief=1: 6 tests from 3 suites, all PASSED
    classification (exe vs launcher/owned/hidden/access)
    ambiguous two-paint windows require explicit index
    single paint auto-resolve; launcher explicit select rejected
    dead/invisible, access-denied message, other-process reject
    HWND dead vs PID/creation-time reuse
Manual setup and exact actions:
  Get-CimInstance Win32_Process filtered for clipstudio|csp: no matching processes
  Start-Process out/build/windows-debug/Debug/TracingApp.exe; wait 2s
  PID 38200, MainWindowTitle=TracingApp, Responding=True, CloseMainWindow=True, ExitCode=0
Expected / actual:
  expected: enumerate top-level windows, classify by process image not title/class alone, explicit select when ambiguous, reject dead/reused HWND, no injection/admin default
  actual: automated policy tests pass; control window launched and closed cleanly
  real CLIPStudioPaint.exe selection and launcher/second-instance replacement check NOT RUN (no CSP process)
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.TargetSelectionTests.exe
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  CLIP STUDIO PAINT / launcher not running; CSP version/locale/theme NOT RECORDED
  Release configure/build/test NOT RUN this step
  geometry, DPI, lifecycle remain Prompt 04
  no process injection; OpenProcess uses PROCESS_QUERY_LIMITED_INFORMATION only
Manual checklist remaining for real CSP:
  1. Start CLIP STUDIO PAINT with a document window (not only the launcher).
  2. Optionally start a second paint instance or leave the launcher open.
  3. Run TracingApp.exe, click Refresh, confirm PAINT vs LAUNCHER rows.
  4. With two PAINT rows, click Select with none highlighted: expect Ambiguous, no attach.
  5. Highlight one PAINT row and Select: status shows PID and generation.
  6. Close that document or replace the HWND and Refresh: expect target cleared, no silent reuse.
Blocker or next prompt: 04 — Physical geometry and lifecycle
```

```text
Step / milestone: 04 / M1 physical geometry and lifecycle
Date / commit: 2026-09-25 / working tree on feature/init ahead of 2bb21dce (uncommitted)
Status: PASS for Debug configure/build/test, embedded PerMonitorV2 manifest, and control-window DPI/geometry smoke; real CSP move/resize/minimize/close NOT RUN
Changed files:
  resources/app.manifest (new)
  src/platform/TargetGeometry.h (new)
  src/platform/TargetGeometry.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 859648 bytes (2026-09-25 16:51:09)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 2, passed 2, failed 0
    TracingApp.BootstrapTests 0.20s
    TracingApp.TargetSelectionTests 0.01s
Manual setup and exact actions:
  Extracted RT_MANIFEST #1 from TracingApp.exe (1286 bytes): dpiAware true/pm, dpiAwareness PerMonitorV2, asInvoker, Windows 10 supportedOS
  Start-Process TracingApp.exe; wait 2s; GetDpiForWindow(MainWindowHandle); GetClientRect+ClientToScreen vs GetWindowRect; CloseMainWindow
  Get-CimInstance Win32_Process filtered for clipstudio|csp: no matching processes
Expected / actual:
  expected: PerMonitorV2 manifest embedded; control window reports physical client bounds separately from outer window; clean exit
  actual: PID 40136, MainWindowTitle=TracingApp, hwnd=0xC0CE0, GetDpiForWindow=96
    client physical origin=(164,187) size=704x601
    outer window (156,156)-(876,796) size=720x640 (client != outer, as required)
    CloseMainWindow=True, ExitCode=0, no leftover TracingApp process
  real CLIPStudioPaint.exe select/move/resize/minimize/restore/close NOT RUN (no CSP process)
  mixed-DPI monitor movement NOT RUN (both displays effective DPI 96)
  negative-origin monitor NOT RUN (no monitor with origin < 0)
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.BootstrapTests.exe
  out/build/windows-debug/Debug/TracingApp.TargetSelectionTests.exe
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  CLIP STUDIO PAINT not running; CSP version/locale/theme NOT RECORDED
  Release configure/build/test NOT RUN this step
  both monitors 96 DPI; no negative-origin display
  geometry policy unit tests deferred (would be a sixth file: tests/unit/TargetGeometryTests.cpp)
  canvas ROI is intentionally not reported; client physical bounds are not canvas bounds
  WinEvent is OUTOFCONTEXT only; no process injection
Manual checklist remaining for real CSP geometry:
  1. Start CLIP STUDIO PAINT with a document window (not only the launcher).
  2. Run TracingApp.exe, Refresh, Select a PAINT row.
  3. Confirm status shows target gen, geom gen, client physical origin/size/DPI, outer window separately, and "not canvas bounds".
  4. Move and resize the painting window; geom generation should increment when client origin, size, or DPI changes.
  5. If a differently scaled monitor exists, drag CSP onto it and record expected vs observed client origin/size/DPI.
  6. If a negative-origin monitor exists, drag CSP onto it and record a negative client origin.
  7. Minimize, restore, then close CSP; eligibility should become minimized then target-dead, and the target must clear (no silent HWND reuse).
Blocker or next prompt: 05 — Shared D3D11 device
```

```text
Step / milestone: 05 / M2 shared D3D11 device (no overlay)
Date / commit: 2026-09-25 / working tree on feature/init ahead of c2557c2 (uncommitted)
Status: PASS for Debug configure/build/test, real hardware device-create/shutdown smoke, and debug-layer live-object capture; overlay/capture NOT RUN
Changed files:
  src/graphics/DeviceResources.h (new)
  src/graphics/DeviceResources.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  tests/unit/GraphicsPolicyTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 883200 bytes (2026-09-25 17:02:19)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 3, passed 3, failed 0
    TracingApp.BootstrapTests 0.64s
    TracingApp.TargetSelectionTests 0.01s
    TracingApp.GraphicsPolicyTests 0.03s
  GraphicsPolicyTests --gtest_brief=1: 8 tests from 2 suites, all PASSED
    debug-layer-missing does not fail; hardware CreateFailed is Failed
    Ready -> Removed -> Retrying -> Ready; retry exhaustion Failed
    Shutdown from Uninitialized/Ready/Removed/Retrying/Failed -> Released
    feature level 11_1/11_0 labels
Manual setup and exact actions:
  Start-Process TracingApp.exe; wait 2s; read Static child text; CloseMainWindow; WaitForExit 5s
  DBWIN_BUFFER listener around a second launch/close to capture ID3D11Debug::ReportLiveDeviceObjects
Expected / actual:
  expected: BGRA-capable hardware D3D11 device, exclusive immediate context, debug layer enabled if installed (absence is not create failure), adapter/feature level/removed-reason in status, clean exit
  actual PID 20784, MainWindowTitle=TracingApp, hwnd=1511794, GetDpiForWindow=96, CloseMainWindow=True, ExitCode=0, no leftover process
    D3D11 adapter="NVIDIA GeForce RTX 3060" vendor=0x10DE device=0x24C7 featureLevel=11_1 state=Ready
    debugLayer requested=yes enabled create=0x00000000 removed=0x00000000 liveObjectsReported=no (report runs on shutdown)
  live-object OutputDebugString on shutdown (appPid 10368):
    D3D11 WARNING: Live ID3D11Device at 0x0000013D92BADF10, Refcount: 2 [ STATE_CREATION WARNING #441: LIVE_DEVICE]
    no live context/texture/buffer names were emitted; LIVE_DEVICE is consistent with ID3D11Debug still holding the device after our ComPtrs were released. Not a GPU reset.
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.GraphicsPolicyTests.exe
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  no swap chain, overlay, DirectComposition, WGC, or OpenCV
  device-removed retry is covered by the injectable policy tests only; no real driver reset was induced
  CLIP STUDIO PAINT not required and not used this step
Manual checklist remaining:
  none for this prompt; overlay/affinity/pass-through are Prompt 06–07
Blocker or next prompt: 06 — Transparent overlay surface and affinity
```

```text
Step / milestone: 06 / M2 transparent overlay surface and affinity (test pattern)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 4b61844 (uncommitted)
Status: PASS for Debug configure/build/test, DirectComposition overlay HWND, WDA_EXCLUDEFROMCAPTURE set+readback, emergency hide, GPU-free visibility policy, test-pattern smoke, and selected-target client placement; OBS Display Capture NOT RUN; input pass-through NOT RUN
Changed files:
  src/graphics/OverlaySurface.h (new)
  src/graphics/OverlaySurface.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  tests/unit/OverlayPolicyTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 935424 bytes (2026-09-25 17:20:00)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 4, passed 4, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
  OverlayPolicyTests --gtest_brief=1: 12 tests from 2 suites, all PASSED
    eligible / control-foreground show
    unrelated foreground hide
    unusable target hide
    affinity failure hide
    emergency latch / clear
    content-not-ready hide
    test-pattern-without-target only when flagged
    test-pattern does not override unrelated foreground
    OBS NOT RUN is not an input and does not authorize show
    affinity ready requires set + matching readback
Manual setup and exact actions:
  1. Start-Process TracingApp.exe (PID 43564 then later  rebuild smoke). Overlay HWND created hidden.
  2. Status before show: overlay visible=no contentReady=yes affinity set=ok win32=0 readback=0x00000011 matchExclude=yes OBS verification=NOT RUN visibility=Hide reason=target-unusable
  3. Overlay exstyle 0x08200080: WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP; GetWindowDisplayAffinity=0x11 (WDA_EXCLUDEFROMCAPTURE); unowned top-level.
  4. Click Show test marker: visible=yes reason=shown-test-pattern placement 240x160 present=0x00000000; GetForegroundWindow stayed TracingAppControlWindow.
  5. Physical mouse click on overlay client: foreground remained TracingAppControlWindow (isOverlay=false).
  6. Click Emergency Hide: visible=no reason=emergency-hidden; latch held across status refresh.
  7. CloseMainWindow=True ExitCode=0 leftover none; overlay HWND destroyed with process.
  8. Live CSP: CLIPStudioPaint.exe 5.0.0 pid 36888 and CLIPStudio.exe launcher pid 20700. Refresh listed 49 rows including multiple [PAINT] rows for pid 36888 and [LAUNCHER] CLIP STUDIO.
  9. Selected a [PAINT] CLIPStudioPaint.exe row (not the titled main frame). Overlay showed on that HWND's client physical origin=(2198,132) size=231x263; overlay window rect (2198,132)-(2429,395) matched. Control foreground: overlay stayed visible. SetForegroundWindow(launcher 0x70462): overlay_vis=False. Restore control: overlay_vis=True. Emergency Hide: overlay_vis=False.
  10. Independently measured titled CLIP STUDIO PAINT hwnd 0x70E9A client origin=(1920,0) size=2560x1439 (DISPLAY1). That row was not the one attached in step 9.
  11. CopyFromScreen of the test-pattern rect saved out/manual/overlay-test-marker.png showing the control-window list with no magenta/yellow L. Expected with WDA_EXCLUDEFROMCAPTURE + WS_EX_NOREDIRECTIONBITMAP; not an OBS Display Capture result and not proof the physical display lacked the marker.
Expected / actual:
  expected: hidden overlay until affinity succeeds; transparent DComp surface; asymmetric test marker; no activation; emergency hide; OBS status separate from affinity; overlay follows selected target client physical bounds when policy allows
  actual: affinity set+readback 0x11 before any show; test-pattern show/hide and no-activate PASS; policy tests PASS; selected-target placement matched that HWND's client physical rect; unrelated-app hide PASS; OBS left NOT RUN
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.OverlayPolicyTests.exe
  out/manual/overlay-test-marker.png (GDI capture omitted overlay; local only)
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  no WS_EX_TRANSPARENT / HTTRANSPARENT / input probe (Prompt 07)
  no imported image renderer, WGC, or OpenCV
  OBS Display Capture exclusion NOT RUN; affinity BOOL is not OBS proof
  mixed-DPI still both 96; no negative-origin monitor
  CSP minimize/restore/close and overlay-over-main-canvas not exercised (did not minimize or close the live painting session)
  multiple PAINT-classified HWNDs exist; overlay tracks the explicitly selected HWND, which may be a small panel rather than the titled CLIP STUDIO PAINT frame
Manual checklist remaining:
  1. Highlight the titled CLIP STUDIO PAINT row, Select, confirm overlay origin/size equals that window's client physical rect.
  2. Minimize/restore/close CSP and confirm hide + no silent HWND reuse.
  3. Prompt 07: mouse/wheel/pen pass-through without activating the overlay.
  4. Prompt 13: saved OBS Display Capture playback.
Blocker or next prompt: 07 — Prove drawing pass-through
```

```text
Step / milestone: 07 / M2 prove drawing pass-through
Date / commit: 2026-09-25 / working tree on feature/init ahead of a8a391d (uncommitted)
Status: FAIL for cross-process pass-through on the DirectComposition HWND. PASS for Debug configure/build/test, InputProbe --self-test, tracing vs alignment style toggle, no overlay activation, and emergency hide. CSP pen/pressure NOT RUN. Do not treat the overlay as usable. Do not start Prompt 08.
Changed files:
  src/graphics/OverlaySurface.h
  src/graphics/OverlaySurface.cpp
  src/app/main.cpp
  tests/harness/InputProbe.cpp (new)
  CMakeLists.txt
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 938496 bytes (2026-09-25 17:35:00)
  TracingApp.InputProbe.exe 69632 bytes (2026-09-25 17:35:02)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 5, passed 5, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.InputProbe (--self-test of probe counters only; not overlay pass-through)
Manual setup and exact actions:
  1. Start TracingApp.InputProbe.exe PID 39412 and TracingApp.exe PID 35772 (different processes).
  2. Probe window class TracingAppInputProbe title "TracingApp InputProbe  down=0 wheel=0 pen=0" client 640x480.
  3. Click Tracing mode, then Cover input probe. Overlay shown over probe client origin=(164,187) size=640x480, reason=shown-test-pattern, affinity readback=0x11, layered=no.
  4. Overlay exstyle 0x082000A8 = WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST (gated topmost-while-shown).
  5. WindowFromPoint at overlay center (484,427) returned TracingAppOverlayWindow even in tracing mode (WS_EX_TRANSPARENT did not skip hit-test).
  6. SendInput left-click and wheel at overlay center in tracing mode: probe stayed down=0 wheel=0; overlay consumed mouseDown=0 wheel=0 pointerDown=0; GetForegroundWindow stayed TracingAppControlWindow (isOverlay=false).
  7. Alignment mode: exstyle transparent=no; SendInput left-click: overlay consumed mouseDown=1; probe still down=0. Input synthesis works; alignment intercepts; tracing does not forward.
  8. Return to tracing: overlay consumed counters reset to 0; another SendInput click still left probe at down=0 (no stuck alignment input; still no pass-through).
  9. Emergency Hide: overlay visible=no reason=emergency-hidden zOrder=not-topmost.
  10. CLIPStudioPaint.exe was not overlaid this step; mouse/wheel/pen on a real CSP canvas NOT RUN.
Expected / actual:
  expected: mouse/wheel/pen reach a different process beneath the overlay without activating it; alignment consumes input and is reversible; emergency hide remains
  actual: no activation and emergency hide PASS; alignment consumes overlay clicks PASS; tracing/alignment toggle PASS; cross-process pass-through FAIL (hits land on the DComp overlay; tracing swallows without delivering to InputProbe)
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.InputProbe.exe
  out/manual/prompt07-passthrough.txt
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  WS_EX_TRANSPARENT + WM_NCHITTEST HTTRANSPARENT on WS_EX_NOREDIRECTIONBITMAP DirectComposition HWND is not sufficient for cross-process click-through on this OS/GPU
  HWND_TOPMOST while shown was required to keep the overlay above the probe; it did not cause overlay activation
  InputProbe --self-test only SendMessages its own HWND; it does not certify overlay pass-through
  CSP canvas drawing and pen pressure NOT RUN
  OBS Display Capture still NOT RUN
  no WS_EX_LAYERED on this path (incompatible with the current DComp swap chain)
Proposed next substep (do not rewrite the renderer wholesale):
  07-fallback — bounded layered-window presentation: keep the existing D3D test-pattern draw, present through an isolated WS_EX_LAYERED HWND (UpdateLayeredWindow or layered swap), apply WS_EX_TRANSPARENT there, measure readback/upload cost, rerun this InputProbe click/wheel/pen matrix, and keep WDA_EXCLUDEFROMCAPTURE + emergency hide. Do not start Prompt 08 until that gate PASSes.
Manual checklist remaining:
  1. After a passing pass-through path exists: repeat InputProbe click/wheel/pen, then CSP mouse/wheel and pen pressure if hardware is present.
  2. Prompt 13: saved OBS Display Capture playback.
Blocker or next prompt: 07-fallback — layered-window pass-through (Prompt 08 blocked)
```

```text
Step / milestone: 07-fallback / M2 layered-window pass-through
Date / commit: 2026-09-25 / working tree on feature/init ahead of 629c625 (uncommitted)
Status: PASS for InputProbe mouse/wheel pass-through on both monitors, titled-CSP HWND hit-test, no overlay activation, emergency hide, affinity set+readback, and Debug configure/build/test. Pen/pressure NOT RUN. Do not treat OBS exclusion as proven.
Changed files:
  src/graphics/OverlaySurface.h
  src/graphics/OverlaySurface.cpp
  src/app/main.cpp
  tests/harness/InputProbe.cpp
  CMakeLists.txt
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 940032 bytes (2026-09-25 18:02:53)
  TracingApp.InputProbe.exe 70144 bytes (2026-09-25 18:02:00)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 5, passed 5, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.InputProbe (--self-test of probe counters only; not overlay pass-through)
Manual setup and exact actions:
  DirectComposition was removed. Overlay is WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, plus WS_EX_TRANSPARENT in tracing. Test pattern is drawn to a BGRA D3D11 texture, copied to a staging resource, and presented with UpdateLayeredWindow (ULW_ALPHA, premultiplied). WDA_EXCLUDEFROMCAPTURE is still applied before show.
  Dual-monitor virtual desktop origin=(0,0) size=4480x1440. DISPLAY2 primary {0,0,1920x1080}; DISPLAY1 {1920,0,2560x1440}.
  1. Start TracingApp.InputProbe.exe PID 18996 and TracingApp.exe PID 46472 (different processes). Park control at (1000,80) so it does not overlap the probe.
  2. Primary DISPLAY2: Cover input probe. Overlay origin=(88,111) size=640x480, layered=yes noredirectionbitmap=no transparent=yes affinity readback=0x11 present=0x00000000 readbackUs=244 ulwUs=75.
  3. WindowFromPoint tracing at (408,351) returned TracingAppInputProbe (not overlay).
  4. SendInput click+wheel at that point: probe down 0->1 and wheel 0->1; overlay consumed stayed 0; foreground became the probe (isOverlay=false).
  5. Alignment mode: WindowFromPoint returned TracingAppOverlayWindow; probe counters unchanged. Overlay WndProc consumed mouseDown stayed 0 for synthesized clicks (layered WS_EX_NOACTIVATE did not deliver WM_LBUTTONDOWN); intercept is the WindowFromPoint result plus probe-not-incrementing.
  6. Return to tracing: another SendInput click incremented probe again (no stuck alignment input).
  7. Emergency Hide: overlay visible=no reason=emergency-hidden.
  8. Secondary DISPLAY1: probe/overlay origin=(2008,111). WindowFromPoint (2328,351) returned InputProbe. Click 2->3 and wheel 1->2; overlay consumed 0. Alignment WindowFromPoint overlay; return-to-tracing click incremented probe; emergency hide again.
  9. Titled CLIP STUDIO PAINT hwnd 0x70E9A pid 36888 selected (not a small panel). Overlay shown-on-target origin=(1920,0) size=2560x1439. WindowFromPoint returned a CSP child hwnd in the same pid; overlay consumed 0; GetForegroundWindow stayed CLIP STUDIO PAINT. Pen/pressure NOT RUN.
Expected / actual:
  expected: mouse/wheel reach a different process beneath the overlay without activating it; alignment intercepts; emergency hide remains; measure readback/upload
  actual: InputProbe click/wheel PASS on DISPLAY2 and DISPLAY1; overlay never became foreground; alignment hit-test intercepts; emergency hide PASS; CSP HWND pass-through PASS; measured GPU staging+row copy and UpdateLayeredWindow microseconds (640x480 typically readbackUs 200-800 / ulwUs 70-100; 2560x1439 CSP rect readbackUs 3922-4955 / ulwUs 706-1595)
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.InputProbe.exe
  out/manual/prompt07-fallback-passthrough.txt
Measured samples / p50 / p95 / max where applicable:
  640x480 test-pattern present: readbackUs samples 244, 542, 765, 923, 1932; ulwUs samples 75, 87, 92, 86, 79
  2560x1439 CSP client present: readbackUs 3922 and 4955; ulwUs 1595 and 706
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  DirectComposition path removed (MASTER forbids mixing layered and composition presenters)
  Alignment SendInput did not increment overlay consumed counters; intercept is proven by WindowFromPoint + probe-not-incrementing
  CSP canvas stroke/pressure NOT RUN (no pen evidence this step)
  OBS Display Capture still NOT RUN; layered HWND affinity must be re-verified in Prompt 13
  mixed-DPI still both 96; no negative-origin monitor
Manual checklist remaining:
  1. Pen/pressure on CSP if a tablet is available.
  2. Prompt 13: saved OBS Display Capture playback on this layered HWND.
Blocker or next prompt: 08 — WGC session and owned frames
```

```text
Step / milestone: 08 / M3 WGC session and owned frames
Date / commit: 2026-09-25 / working tree on feature/init ahead of 3197494 (uncommitted)
Status: PASS for Debug configure/build/test, GPU-free capture state/handoff tests, WGC CreateForWindow on titled CLIP STUDIO PAINT, owned-frame counters/content-size change on resize/restore, and Stop. No capture preview (Prompt 09). Overlay test-pattern is not labeled as a captured frame.
Changed files:
  src/capture/CaptureSession.h (new)
  src/capture/CaptureSession.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  tests/unit/CaptureStateTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 1983488 bytes (2026-09-25 18:35:56)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 6, passed 6, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.CaptureStateTests
    TracingApp.InputProbe (--self-test of probe counters only)
  CaptureStateTests --gtest_brief=1: 8 tests from 3 suites, all PASSED
    SupportMissing is Unsupported, not Failed
    Start then Stop rejects further frames
    ItemClosed invalidates generation
    wrong generation leaves accepted unchanged
    pending bound drops oldest on third arrival
    zero content size is stale, not accepted
    sequence increments only on accept
    state/action labels
Manual setup and exact actions:
  1. CLIPStudioPaint.exe 5.0.0 pid 36888 already running; titled HWND 0x70E9A client origin=(1920,0) size=2560x1439 on DISPLAY1.
  2. Nested SendMessage BM_CLICK of Start Capture first hit GraphicsCaptureSession::IsSupported with HRESULT 0x8001010D (RPC_E_WRONG_THREAD) and was treated as Unsupported. Start/Stop were then posted to the UI GetMessage loop.
  3. Start TracingApp.exe PID 44504. Refresh, highlight list row [PAINT] CLIPStudioPaint.exe | CLIP STUDIO PAINT, Select, Start Capture.
  4. Before resize: WGC supported=yes state=Running sessionGen=1 accepted=1 lastSeq=1 contentSize=2576x1456 ownedFrame=yes lastHr=0. Overlay report still says test pattern, not imported image / not a captured frame.
  5. SetWindowPos reduced the CSP outer rect 2576x1456 -> 2376x1256. After 2s: accepted=14 lastSeq=14 contentSize=2376x1256 captureTicks changed.
  6. Restored 2576x1456. After 2s: accepted=18 lastSeq=18 contentSize=2576x1456.
  7. Stop Capture: state=Stopped ownedFrame=no accepted stayed 18.
  8. Process CloseMainWindow while the overlay covered CSP did not exit in 8s (force-killed). Separate smoke: PostMessage WM_CLOSE to TracingAppControlWindow without capture exited 0 with no leftover.
Expected / actual:
  expected: CreateForWindow + free-threaded pool; owned copy on graphics thread; counters and content size change on a real CSP window; stop/item-closed; tests without GPU; overlay test-pattern never called a captured frame
  actual: matches for counters/metadata on titled CSP HWND; WGC content size followed outer window (2576x1456) not client (2560x1439) while maximized; static window produced one frame until resize dirtied capture; no preview this step
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.CaptureStateTests.exe
  out/manual/prompt08-wgc.txt
Measured samples / p50 / p95 / max where applicable: n/a (sequence/content-size only)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  frame-pool recreation on content-size change deferred to Prompt 09
  capture preview deferred to Prompt 09; overlay-in-WGC-texture check deferred to Prompt 09
  WGC content bounds are not client bounds; mapping remains Prompt 09
  static CSP contents did not keep producing frames until the window was resized
  item-closed path not exercised (CSP was not closed)
  nested SendMessage into WndProc is not a valid WinRT calling context; UI posts Start/Stop onto the message loop
Manual checklist remaining:
  1. Prompt 09: preview actual WGC frames, pool recreate, stale/zero labels, overlay absent from CSP capture.
  2. Close CSP while capturing and confirm item-closed.
  3. Prompt 13: saved OBS Display Capture playback.
Blocker or next prompt: 09 — Capture preview, resize and stale-frame handling
```

```text
Step / milestone: 09 / M3 capture preview, resize, stale-frame handling
Date / commit: 2026-09-25 / working tree on feature/init ahead of 9edfc8a (uncommitted)
Status: PASS for Debug configure/build/test, opt-in ordinary WDA_NONE preview of actual WGC frames (off by default), pool Recreate on CSP resize, measured capture-to-client mapping, overlay hide on minimize, and magenta marker absent from owned CSP frames. Item-closed by closing CSP NOT RUN. Zero-size stale label not observed (minimize kept last content size).
Changed files:
  src/capture/CaptureSession.h
  src/capture/CaptureSession.cpp
  src/graphics/CapturePreview.h (new)
  src/graphics/CapturePreview.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2033664 bytes (2026-09-25 18:55:23)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 6, passed 6, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.CaptureStateTests
    TracingApp.InputProbe (--self-test of probe counters only)
Manual setup and exact actions:
  CLIPStudioPaint.exe 5.0.0 pid 36888; titled HWND 0x70E9A on DISPLAY1. Start TracingApp.exe PID 42732. Refresh, select list row [PAINT] CLIPStudioPaint.exe | CLIP STUDIO PAINT, Start Capture, then Enable capture preview, Show test marker, resize/restore CSP, minimize/restore CSP. Close TracingApp via WM_CLOSE (exit 0). Did not close CSP.
Expected / actual:
  expected: preview HWND hidden until checkbox; actual WGC frames (not test-pattern); content-size Recreate; capture vs client mapping measured not hardcoded; STALE/ZERO labels; hide overlay on capture loss; magenta L not in CSP texture
  actual:
    after Start Capture, preview enabled=no visible=no checkbox=0; WGC Running accepted=5 contentSize=2576x1456 ownedFrame=yes recreates=0
    after Enable capture preview: preview enabled=yes visible=yes label=ACTUAL seq=8 bitmap=2576x1456 markerFeedback=no
    after Show test marker: overlay shown-on-target over CSP; markerFeedback still no (magenta overlay not in owned WGC copy)
    mapping on this maximized HWND: match=outer-and-dwm content=outer=dwm=client=2576x1456 usingOffset=(0,0) residual=0x0 (not canvas bounds). Prompt 08 had client 2560x1439 vs WGC 2576x1456 on a restored chrome window; this session's QueryPhysicalGeometry client equals DWM/outer.
    resize 2576x1456 -> 2376x1256: accepted 8->14 contentSize=2376x1256 poolSize=2376x1256 recreates=1 recreateHr=0; preview bitmap followed
    restore 2576x1456: accepted=18 recreates=2 geomGen=3
    minimize: overlay visibility=Hide reason=target-unusable; WGC kept last contentSize 2576x1456 (no STALE/ZERO event); eligibility=minimized
    restore: overlay shown-on-target; accepted=23 recreates=4 geomGen=5
    overlayHiddenOnCaptureLoss stayed no (minimize is geometry hide, not WGC item-closed/stale)
    second launch: accepted 1->2 after canvas interaction; preview ACTUAL seq=2 markerFeedback=no; WM_CLOSE exit 0
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/manual/prompt09-preview.txt
Measured samples / p50 / p95 / max where applicable: n/a (sequence/content-size/recreate count)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  CSP close-while-capturing / item-closed NOT RUN (live document not closed)
  zero-size stale label NOT observed; minimize did not produce a 0x0 WGC content size
  mixed-DPI still both 96; no negative-origin monitor (maximized CSP client origin Y=-8 is DWM overlap, not a negative monitor origin)
  preview is a debug WDA_NONE window; must stay off for OBS recording (Prompt 13)
  markerFeedback=no is WGC-of-CSP proof only, not OBS Display Capture proof
Manual checklist remaining:
  1. Close CSP while capturing and confirm item-closed plus overlay hide latch.
  2. Prompt 10: WIC image import.
  3. Prompt 13: saved OBS Display Capture playback.
Blocker or next prompt: 10 — WIC image import
```

```text
Step / milestone: 10 / M4 WIC image import (decode only)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 9e82224 (uncommitted)
Status: PASS for Debug configure/build/test, generated PNG/JPEG/alpha/Unicode/corrupt/BMP-reject/oversized-IHDR/slot-preserve tests, EXIF orientation 6 roundtrip, and IFileOpenDialog import of valid PNG/JPEG plus corrupt keep-previous and Unicode path. Overlay remains test-pattern (not uploaded). Ordinary ReferenceWindow and OBS remain later prompts.
Changed files:
  src/image/ImageLoader.h (new)
  src/image/ImageLoader.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  tests/unit/ImageLoaderTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  first cmake --build --preset windows-debug: exit 1
    windows.h min/max macros vs std::numeric_limits::max; UNICODE DeleteFile name clash in tests
  cmake --build --preset windows-debug (after INT_MAX/UINT64_MAX and RemoveTempFile)
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2149888 bytes (2026-09-25 19:30:20)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 7, passed 7, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.CaptureStateTests
    TracingApp.ImageLoaderTests
    TracingApp.InputProbe (--self-test of probe counters only)
  ImageLoaderTests: 14 tests from 3 suites, all PASSED (28 ms)
    ImageLimits: zero/negative, axis>16384, 16384x1 and 8192x8192 at 256 MiB, 8193x8192 over budget
    ImageOrientationMap: EXIF 1-8 to WIC transform flags; leaf 测试.png
    ImageComTest: PNG premul A=128 -> BGRA 0,0,128,128; JPEG opaque A=255; Unicode path; corrupt fail;
      BMP unsupported-container WINCODEC_ERR_UNKNOWNIMAGEFORMAT; IHDR width=20000 rejected;
      LoadedImageSlot keeps previous; JPEG EXIF orientation 6 swapped 4x1 -> 1x4 (not skipped);
      empty path E_INVALIDARG
Manual setup and exact actions:
  Generated local ignored fixtures (not committed): out/manual/prompt10-valid.png 4x3, prompt10-valid.jpg 4x3, prompt10-corrupt.png 7 bytes, out/manual/unicodé-路径/测试.png.
  1. Start TracingApp.exe PID 32464. Status image loaded=no. CloseMainWindow=True ExitCode=0.
  2. Start TracingApp.exe. Click Import image (IFileOpenDialog title "Import reference PNG or JPEG").
  3. Open prompt10-valid.png: loaded=yes leaf="prompt10-valid.png" size=4x3 stride=16 format=32bppPBGRA premultiplied container=png bytes=48. Overlay report still "test pattern, not imported image".
  4. Open prompt10-valid.jpg: replaced, container=jpeg size=4x3.
  5. Open prompt10-corrupt.png: lastHr=0x88982F50 reason=decoder-create-failed; previous jpeg preserved.
  6. Open unicodé-路径/测试.png: loaded leaf="测试.png" container=png size=4x3.
  7. Import cancelled (ESC): 测试.png still loaded.
  8. CloseMainWindow=True ExitCode=0 leftover none.
Expected / actual:
  expected: Unicode file selection; WIC PNG/JPEG to owned 32bppPBGRA premul once; 16384/axis and 256 MiB checked before alloc; fail keeps previous; decode independent of overlay
  actual: matches; status explicitly "decode only; not uploaded to overlay"
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.ImageLoaderTests.exe
  out/manual/prompt10-import.txt
  out/manual/prompt10-valid.png
  out/manual/prompt10-valid.jpg
  out/manual/prompt10-corrupt.png
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  imported pixels are not drawn on the overlay (Prompt 11)
  ordinary ReferenceWindow not added (Prompt 12)
  OBS Display Capture still NOT RUN (Prompt 13)
  mixed-DPI still both 96; no negative-origin monitor
Manual checklist remaining:
  1. Prompt 11: upload decoded image to overlay renderer.
  2. Prompt 13: saved OBS Display Capture playback.
Blocker or next prompt: 11 — Image renderer
```

```text
Step / milestone: 11 / M4 image renderer (upload + textured quad on existing overlay)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 38ff8b1 (uncommitted)
Status: PASS for Debug configure/build/test, discovered fxc compile of shaders/Image.hlsl, immutable GPU upload, overlay ULW present of PNG/JPEG, opacity 0/0.5/1, Fit/Reset without re-upload, and corrupt keep-previous GPU texture. Physical eyeball of PNG fringe vs checkerboard NOT photographed (WDA_EXCLUDEFROMCAPTURE blocks GDI CopyFromScreen of the overlay, same as Prompt 06). CSP overlay/pen NOT RUN.
Changed files:
  src/graphics/ImageRenderer.h (new)
  src/graphics/ImageRenderer.cpp (new)
  shaders/Image.hlsl (new)
  src/app/main.cpp
  CMakeLists.txt
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  first cmake --preset windows-debug: exit 1
    fxc.exe not found; WindowsSdkVerBinPath/WindowsSdkDir unset in this shell (missing-tool, not a source compile failure)
  cmake --preset windows-debug after KitsRoot10 / CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION hints
  exit 0
    TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
    reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  fxc compiled shaders/Image.hlsl -> out/build/windows-debug/shaders/ImageVS.cso and ImagePS.cso; POST_BUILD copied next to TracingApp.exe
  TracingApp.exe 2209792 bytes (2026-09-25 20:02:09)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 7, passed 7, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.CaptureStateTests
    TracingApp.ImageLoaderTests
    TracingApp.InputProbe (--self-test of probe counters only)
Manual setup and exact actions:
  Generated ignored fixtures: out/manual/prompt11-checker-asymmetric.png 64x64 (transparent corners, magenta bar, yellow square, checkerboard), prompt11-opaque.jpg 48x32 (opaque BGR bands), prompt11-corrupt.png 7 bytes.
  1. Start TracingApp.exe PID 3156 smoke: pipeline=yes uploaded=no vs=...\Debug\ImageVS.cso lastHr=0; CloseMainWindow ExitCode=0.
  2. Start TracingApp.exe PID 42652. IFileOpenDialog title "Import reference PNG or JPEG".
  3. Open prompt11-checker-asymmetric.png: image loaded 64x64 png; imageRenderer uploaded=yes generation=1 size=64x64 lastHr=0. Overlay still hidden (no target / no show yet).
  4. Show test marker: overlay visible=yes reason=shown-test-pattern placement 240x160 origin=(248,248); imageRenderer lastHr=0 readbackUs=954 ulwUs=76 (image present overwrote the marker ULW).
  5. Fit: placement offset=(40.00,0.00) scale=2.50000 (64*2.5=160 height contain in 240x160); textureGeneration=1 unchanged.
  6. Reset: offset=(0,0) scale=1; generation=1 unchanged.
  7. Opacity trackbar 0 / 50 / 100: opacity=0.000 / 0.500 / 1.000; generation stayed 1.
  8. Open prompt11-opaque.jpg: replaced CPU+GPU, container=jpeg size=48x32 generation=2 scale=5.00000 (32*5=160 contain).
  9. Open prompt11-corrupt.png: lastHr=0x88982F50 decoder-create-failed; previous jpeg CPU preserved; GPU generation stayed 2 size=48x32.
  10. WM_CLOSE exit 0 leftover none.
Expected / actual:
  expected: build-time fxc, upload once, premul textured quad on existing overlay, opacity/fit/reset without touching the texture, fail keeps previous GPU texture
  actual: matches for Debug present/status; OverlaySurface.cpp still draws the marker first (five-file cap); ImageLoader status line still says "decode only" (that file was out of scope)
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/ImageVS.cso
  out/build/windows-debug/Debug/ImagePS.cso
  out/manual/prompt11-import.txt
  out/manual/prompt11-checker-asymmetric.png
  out/manual/prompt11-opaque.jpg
  out/manual/prompt11-corrupt.png
Measured samples / p50 / p95 / max where applicable:
  240x160 image present after show: readbackUs 954 then Fit 281 / Reset 851; ulwUs 76 / 50 / 54
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  first configure in a shell without WindowsSdkDir failed until KitsRoot10 discovery was added
  Release configure/build/test NOT RUN this step
  OverlaySurface.cpp not edited; marker draw still runs then ImageRenderer overwrites ULW
  physical PNG-edge fringe inspection vs checkerboard not photographed; GDI CopyFromScreen omits WDA_EXCLUDEFROMCAPTURE HWNDs
  CLIP STUDIO PAINT not running this step; overlay-over-CSP and pen NOT RUN
  ordinary ReferenceWindow not added (Prompt 12)
  OBS Display Capture still NOT RUN (Prompt 13)
  mixed-DPI still both 96; no negative-origin monitor
  ImageLoader FormatReport still says "decode only; not uploaded to overlay"
Manual checklist remaining:
  1. Prompt 12: ordinary ReferenceWindow reusing the reference texture.
  2. Prompt 13: saved OBS Display Capture playback.
  3. Optional later: fold ImageRenderer draw into OverlaySurface::DrawMarker to skip the marker pass.
Blocker or next prompt: 12 — Ordinary reference window
```

```text
Step / milestone: 12 / M4 ordinary reference window
Date / commit: 2026-09-25 / working tree on feature/init ahead of 38ff8b1 (uncommitted; Prompt 11 files also still uncommitted)
Status: PASS for Debug configure/build/test, ordinary WDA_NONE ReferenceWindow sharing the immutable GPU texture via DrawQuad + independent DXGI swap chain, overlay affinity remaining WDA_EXCLUDEFROMCAPTURE, independent fit/opacity, close-reference does not stop WGC, overlay owner-foreground includes the reference HWND, unrelated-app hide leaves the reference visible, emergency hide overlay-only, and capture preview staying off. OBS Display Capture NOT RUN.
Changed files:
  src/graphics/ImageRenderer.h
  src/graphics/ImageRenderer.cpp
  src/app/ReferenceWindow.h (new)
  src/app/ReferenceWindow.cpp (new)
  src/app/main.cpp
  CMakeLists.txt
  project-docs/VALIDATION.md
  (user-approved sixth implementation file: ImageRenderer.h, required to expose ShaderResourceView/DrawQuad)
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2241536 bytes (2026-09-25 20:30:55)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 7, passed 7, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests
    TracingApp.CaptureStateTests
    TracingApp.ImageLoaderTests
    TracingApp.InputProbe (--self-test of probe counters only)
Manual setup and exact actions:
  CLIPStudioPaint.exe 5.0.0 pid 36888; titled HWND selected from list row [PAINT] CLIPStudioPaint.exe | CLIP STUDIO PAINT.
  Fixtures: out/manual/prompt11-checker-asymmetric.png 64x64, prompt11-opaque.jpg 48x32.
  1. Launch TracingApp.exe. Hidden ReferenceWindow class TracingAppReferenceWindow exists at create; GetWindowDisplayAffinity overlay=0x11 reference=0x0; capture preview hidden; Enable capture preview checkbox=0.
  2. Import PNG: reference shown visible=yes matchNone=yes lastHr=0 textureGeneration=1 independentFit scale=6.89062 on client 624x441; overlay image placement scale=22.75000 on 2576x1456; preview still enabled=no checkbox=0.
  3. Overlay Fit: overlay scale stayed 22.75000 (already contain-fit); reference scale stayed 6.89062 (independent). Opacity trackbar 50: overlay opacity=0.500; reference opacity=1.000 generation=1 unchanged.
  4. Start Capture: WGC Running accepted=1 sessionGen=1. WM_CLOSE on reference: visible=no, TracingApp still running, WGC still Running accepted=1 captureStopped=false. Show reference restores the same HWND.
  5. SetForegroundWindow(Shell_TrayWnd 0x2017E): overlay vis=no reason=unrelated-foreground; reference vis=yes; WGC still Running. Control foreground: overlay shown-on-target; reference vis=yes. Reference foreground: overlay shown-on-target (owner-window exception); reference vis=yes.
  6. Emergency Hide: overlay vis=no reason=emergency-hidden; reference vis=yes.
  7. Import JPEG while emergency-hidden: GPU generation=2 size=48x32; reference independentFit scale=13.00000 textureGeneration=2 lastHr=0; overlay stayed emergency-hidden; WGC still Running. WM_CLOSE control exit 0 leftover none.
Expected / actual:
  expected: separate ordinary WDA_NONE reference HWND reusing the uploaded texture with independent presentation; overlay keep exclude-from-capture; closing reference does not stop capture; focus follows overlay visibility policy; capture preview stays off
  actual: matches for Debug HWND/affinity/status/WGC; both windows consumed the same texture generations (1 then 2); OverlaySurface status line still says "test pattern, not imported image" because OverlaySurface.cpp was out of scope (ImageRenderer still overwrites ULW when a texture exists)
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/manual/prompt12-reference.txt
  out/manual/prompt12-csp-focus.txt
  out/manual/prompt11-checker-asymmetric.png
  out/manual/prompt11-opaque.jpg
Measured samples / p50 / p95 / max where applicable: n/a
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  OverlaySurface.cpp not edited; marker draw still runs then ImageRenderer overwrites ULW; overlay FormatReport still labels the surface as test pattern
  ImageLoader FormatReport still says "decode only; not uploaded to overlay"
  physical PNG-edge photograph of the reference window not saved this step; Present lastHr=0x00000000 and independent fit/generation are the recorded evidence
  WGC accepted stayed 1 on a static CSP canvas (same Prompt 08 observation)
  mixed-DPI still both 96; no negative-origin monitor
  pen/pressure NOT RUN
  OBS Display Capture still NOT RUN (Prompt 13)
Manual checklist remaining:
  1. Prompt 13: saved OBS Display Capture playback with magenta overlay marker vs green/ordinary reference.
  2. Optional later: fold ImageRenderer draw into OverlaySurface::DrawMarker to skip the marker pass.
Blocker or next prompt: 13 — OBS Display Capture exclusion gate
```

```text
Step / milestone: 13 / M4 OBS Display Capture exclusion gate
Date / commit: 2026-09-25 / working tree on feature/init ahead of 5008e47 (uncommitted diagnostic)
Status: BLOCKED. Debug configure/build/test PASS. Ordinary WDA_NONE ReferenceWindow appears in saved Display Capture. Magenta WS_EX_LAYERED overlay never appears in saved playback, including WDA_NONE positive control on Automatic, DXGI Desktop Duplication, and Windows 10 (1903 and up). CSP remains intact with no black placeholder. API readback is not OBS proof; positive control FAIL means exclusion cannot be certified. Do not start Prompt 14.
Changed files:
  src/graphics/OverlaySurface.h
  src/graphics/OverlaySurface.cpp
  src/app/main.cpp
  tests/unit/OverlayPolicyTests.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2251776 bytes (2026-09-25 20:52:14)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 7, passed 7, failed 0
    TracingApp.BootstrapTests
    TracingApp.TargetSelectionTests
    TracingApp.GraphicsPolicyTests
    TracingApp.OverlayPolicyTests (14 tests: previous visibility plus TemporaryNoneReadyOnlyOnNoneReadback and RestoredExcludeRequiresExcludeReadback)
    TracingApp.CaptureStateTests
    TracingApp.ImageLoaderTests
    TracingApp.InputProbe (--self-test of probe counters only)
Manual setup and exact actions:
  CLIPStudioPaint.exe 5.0.0 pid 36888; list row [PAINT] CLIPStudioPaint.exe | CLIP STUDIO PAINT selected. Overlay covering client origin=(1912,-8) size=2576x1456 on DISPLAY1/R27qe. Capture preview checkbox remained off.
  Generated ignored fixture out/manual/prompt13-green.png 64x64 asymmetric green L.
  1. Diagnostic UI: OBS +ve WDA_NONE (temp), Skip overlay image (OBS), Recreate overlay HWND.
  2. Skip overlay image checked; Import prompt13-green.png; Show reference moved to (2280,380) 520x380 on R27qe; overlay skip-image-present=yes so magenta/yellow test marker should remain.
  3. OBS 32.2.2 scene Main: Display Capture `Main Monitor` only (Game Capture/CS2/Browser/bongobs cat hidden). Display=R27qe 2560x1440 @ 1920,0. OBS window on primary DISPLAY2. Local recordings to I:/Videos/Obs/GD (HEVC 1920x1080 60fps), copies under out/manual/.
  4. Positive control: checkbox latched overlay readback=0x00000000 matchNone=yes mode=temporary-none-positive-control. Recorded Automatic, DXGI Desktop Duplication, and Windows 10 (1903 and up) separately. Inspected saved playback frames, not only preview.
  5. Restored exclude: overlay readback=0x00000011 matchExclude=yes mode=exclude-from-capture.
  6. Exclude session on Windows 10 (1903 and up), 53.282s: emergency hide, show marker, Recreate overlay HWND, skip-image/opacity 0.5/0/1, CSP resize 2200x1200 then restore. Method restored to Automatic afterward.
Expected / actual:
  expected: WDA_NONE recording contains both magenta overlay and green reference; restored exclude recording contains CSP + green reference only, no magenta, no black hole, including hide/show/recreate/resize
  actual: green TracingApp Reference present in every inspected saved frame; CSP canvas intact; no black overlay placeholder; magenta/yellow overlay marker absent from Automatic, DXGI, and WGC playback even while WDA_NONE readback succeeded. GDI CopyFromScreen of the overlay rect while WDA_NONE also omitted layered pixels (same BitBlt limitation as Prompt 11). Positive control FAIL. Exclusion not certifiable.
Evidence paths (local, no private artwork committed):
  out/build/windows-debug/Debug/TracingApp.exe
  out/manual/prompt13-green.png
  out/manual/prompt13-positive-auto.mp4 (copy of I:/Videos/Obs/GD/2026-09-25 21-03-56.mp4, 21.915s)
  out/manual/prompt13-positive-dxgi.mp4 (copy of I:/Videos/Obs/GD/2026-09-25 21-07-42.mp4, ~8s)
  out/manual/prompt13-positive-wgc.mp4 (copy of I:/Videos/Obs/GD/2026-09-25 21-09-21.mp4, 7.97s)
  out/manual/prompt13-exclude-wgc.mp4 (copy of I:/Videos/Obs/GD/2026-09-25 21-11-05.mp4, 53.282s)
  out/manual/prompt13-positive-frame10.png
  out/manual/prompt13-positive-green-crop.png
  out/manual/prompt13-positive-magenta-crop.png
  out/manual/prompt13-positive-dxgi-frame.png
  out/manual/prompt13-positive-wgc-frame.png
  out/manual/prompt13-exclude-t02.png
  out/manual/prompt13-exclude-t07-hidden.png
  out/manual/prompt13-exclude-t12-shown.png
  out/manual/prompt13-exclude-t18-recreate.png
  out/manual/prompt13-exclude-t28-opacity.png
  out/manual/prompt13-exclude-t38-resize.png
  out/manual/prompt13-exclude-t50.png
  out/manual/prompt13-gdi-overlay-none.png
  out/manual/prompt13-gdi-reference.png
Measured samples / p50 / p95 / max where applicable: n/a (presence/absence of markers, not tracking error)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  mixed-DPI still both 96; no negative-origin monitor
  pen/pressure NOT RUN
  GDI CopyFromScreen does not image WS_EX_LAYERED ULW content even at WDA_NONE (prompt13-gdi-overlay-none.png is CSP chrome, not the magenta L)
  OBS canvas 1920x1080 letterboxed/scaled the 2560x1440 capture
  DXGI and WGC short positive-control files hashed identical (static HEVC of an unchanged scene); Automatic 21.9s file is distinct
  Exclude hide/show/recreate/opacity/CSP-resize was recorded on Windows 10 (1903 and up) only, 53.282s (short of 60s). Automatic and DXGI exclude sessions were not repeated because WDA_NONE positive-control playback on those methods already omitted the overlay, so exclusion cannot be certified
  Exclude stills t02/t07/t12/t18/t28 are bit-identical 462718-byte HEVC frames of an unchanged captured scene; t38 (CSP resize, other desktop visible) and t50 (restored) differ
  HWND recreate and opacity were exercised during the exclude recording; overlay still never appeared in playback
  physical camera photograph of the magenta marker on the panel was not taken
Manual checklist remaining:
  1. Do not start Prompt 14 while this OBS overlay positive-control FAIL stands.
  2. A later dedicated present-path experiment would be required before treating Display Capture exclusion as supported (layered ULW HWND is invisible to the tested OBS Display Capture methods even with WDA_NONE).
Blocker or next prompt: STOP — M4 OBS overlay exclusion BLOCKED (Prompt 14 blocked)
```

```text
OS / GPU / driver / monitor / DPI / HDR:
  Windows 10 Pro 25H2 build 26200.9457 SDR (P709 G22); NVIDIA RTX 3060 32.0.16.1656; R27qe 2560x1440 @ (1920,0) 96 DPI 180Hz
OBS version / Display Capture method label / settings:
  OBS 32.2.2; Display Capture `Main Monitor`; method Automatic; display R27qe 2560x1440 @ 1920,0; cursor on; Force SDR off; 1920x1080 60fps HEVC
CSP version / app commit / renderer path:
  CLIPStudioPaint 5.0.0; TracingApp Debug layered ULW overlay + DXGI reference; skip-image-present=yes
Private HWND affinity set result / readback / error:
  positive-control set=ok win32=0 readback=0x00000000 matchNone=yes; later restored readback=0x00000011
Positive control: FAIL
Both markers physically visible: green PASS in capture; magenta independent photograph NOT RUN (app visible=yes; GDI omitted layered pixels)
Saved recording playback: FAIL (green yes, magenta never)
Private marker absent, including transitions: yes in this file, but not a certified exclude (same absence during WDA_NONE)
Underlying CSP intact (no black replacement): PASS
Ordinary reference visible: PASS
Recreated HWND / moved monitor retest: not in this Automatic clip
Recording path / inspected timestamps:
  I:/Videos/Obs/GD/2026-09-25 21-03-56.mp4 duration 21.915s; inspected ~10s still + green/magenta crops
Tested by / date: Prompt 13 2026-09-25
Supported only for this configuration: NO
```

```text
OS / GPU / driver / monitor / DPI / HDR:
  Windows 10 Pro 25H2 build 26200.9457 SDR (P709 G22); NVIDIA RTX 3060 32.0.16.1656; R27qe 2560x1440 @ (1920,0) 96 DPI 180Hz
OBS version / Display Capture method label / settings:
  OBS 32.2.2; Display Capture `Main Monitor`; method DXGI Desktop Duplication; display R27qe 2560x1440 @ 1920,0; cursor on; Force SDR off; 1920x1080 60fps HEVC
CSP version / app commit / renderer path:
  CLIPStudioPaint 5.0.0; TracingApp Debug layered ULW overlay + DXGI reference; skip-image-present=yes; overlay still WDA_NONE for this clip
Private HWND affinity set result / readback / error:
  set=ok win32=0 readback=0x00000000 matchNone=yes
Positive control: FAIL
Both markers physically visible: green PASS in capture; magenta independent photograph NOT RUN
Saved recording playback: FAIL (green yes, magenta never)
Private marker absent, including transitions: yes, not certified exclude
Underlying CSP intact (no black replacement): PASS
Ordinary reference visible: PASS
Recreated HWND / moved monitor retest: not in this clip
Recording path / inspected timestamps:
  I:/Videos/Obs/GD/2026-09-25 21-07-42.mp4 ~8s; inspected ~4s still + crops
Tested by / date: Prompt 13 2026-09-25
Supported only for this configuration: NO
```

```text
OS / GPU / driver / monitor / DPI / HDR:
  Windows 10 Pro 25H2 build 26200.9457 SDR (P709 G22); NVIDIA RTX 3060 32.0.16.1656; R27qe 2560x1440 @ (1920,0) 96 DPI 180Hz
OBS version / Display Capture method label / settings:
  OBS 32.2.2; Display Capture `Main Monitor`; method Windows 10 (1903 and up); display R27qe 2560x1440 @ 1920,0; cursor on; Force SDR off; 1920x1080 60fps HEVC
CSP version / app commit / renderer path:
  CLIPStudioPaint 5.0.0; TracingApp Debug layered ULW overlay + DXGI reference
Private HWND affinity set result / readback / error:
  positive-control clip: readback=0x00000000 matchNone=yes; exclude clip 53.282s: readback=0x00000011 matchExclude=yes
Positive control: FAIL
Both markers physically visible: green PASS in capture; magenta independent photograph NOT RUN
Saved recording playback: FAIL for overlay positive control; exclude clip still has green reference + intact CSP + no magenta + no black hole after hide/show/recreate/opacity/CSP resize (t38/t50)
Private marker absent, including transitions: yes in exclude playback, not certified (same as positive control)
Underlying CSP intact (no black replacement): PASS
Ordinary reference visible: PASS (still present after CSP resize at t38)
Recreated HWND / moved monitor retest: Recreate overlay HWND posted during exclude recording; monitor not moved; overlay still absent from playback
Recording path / inspected timestamps:
  positive I:/Videos/Obs/GD/2026-09-25 21-09-21.mp4 7.97s
  exclude I:/Videos/Obs/GD/2026-09-25 21-11-05.mp4 53.282s; frames t38 resize and t50 restore
Tested by / date: Prompt 13 2026-09-25
Supported only for this configuration: NO
```

```text
Step / milestone: 13-present / M4 DXGI HWND overlay present-path repair
Date / commit: 2026-09-25 / working tree on feature/init ahead of cb11520 (uncommitted)
Status: PASS for Debug configure/build/test, dxgi-hwnd present (not ULW, not DComp), DISPLAY1 InputProbe click/wheel, Automatic OBS WDA_NONE positive control, and Automatic exclude (no magenta, no black hole). Premultiplied swap-chain create FAIL DXGI_ERROR_INVALID_CALL; ignore-alpha used. Visual transparency FAIL (opaque black). DXGI DDA and Windows 10 (1903+) OBS methods NOT RUN. Prompt 14 unblocked for Automatic on this machine.
Changed files:
  src/graphics/OverlaySurface.h
  src/graphics/OverlaySurface.cpp
  src/graphics/ImageRenderer.h
  src/graphics/ImageRenderer.cpp
  src/app/main.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2251776 bytes (2026-09-25 21:34)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 7, passed 7, failed 0
Manual setup and exact actions:
  Overlay styles: WS_EX_LAYERED | TOOLWINDOW | NOACTIVATE, tracing TRANSPARENT, noredirectionbitmap=no. SetLayeredWindowAttributes(LWA_ALPHA=255). CreateSwapChainForHwnd BGRA FLIP_DISCARD; premulCreateHr=0x887A0001; ignoreCreateHr=0; alphaMode=ignore; dwmExtendFrame=yes; Present=0.
  1. InputProbe + Cover input probe. DISPLAY1/R27qe origin~(2008,111) size 644x481: tracing WindowFromPoint=TracingAppInputProbe, title down=1 wheel=1, overlay not foreground. Alignment hit=TracingAppOverlayWindow. Emergency hide hides overlay.
  2. DISPLAY2: overlay visible; alignment hit overlay; tracing click hit MozillaDialogClass (Firefox dialog overlap); overlay did not activate.
  3. PrintWindow(PW_RENDERFULLCONTENT) and BitBlt CAPTUREBLT of overlay rect: magentaHits=3600, asymmetric magenta/yellow L on opaque black (out/manual/prompt13-present-printwindow.png, prompt13-present-captureblt.png). GDI CopyFromScreen without CAPTUREBLT still omitted overlay pixels.
  4. OBS 32.2.2 scene Main, Display Capture R27qe Automatic, preview on DISPLAY2. Skip overlay image checked. Positive: WDA_NONE readback=0x0 matchNone=yes. Exclude: readback=0x11 matchExclude=yes. Overlay remained physically visible during both.
Expected / actual:
  expected: WDA_NONE saved playback contains magenta overlay; exclude playback omits it with underlying content intact and no black hole
  actual: Automatic positive 9.932s still at t~2s: 1920x1080 magentaHits=2016 yellowHits=1378 (crop shows InputProbe chrome plus magenta L and black body). Automatic exclude 11.932s still at t~3s: magenta=0 yellow=0; crop shows full InputProbe teal client and CSP toolbar, not a black rectangle.
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/manual/prompt13-present-inputprobe2.txt
  out/manual/prompt13-present-printwindow.png
  out/manual/prompt13-present-captureblt.png
  out/manual/prompt13-present-positive-auto.mp4 (copy of I:/Videos/Obs/GD/2026-09-25 21-40-35.mp4)
  out/manual/prompt13-present-positive-auto-frame.png
  out/manual/prompt13-present-positive-auto-magenta-crop.png
  out/manual/prompt13-present-exclude-auto.mp4 (copy of I:/Videos/Obs/GD/2026-09-25 21-45-38.mp4)
  out/manual/prompt13-present-exclude-auto-frame.png
  out/manual/prompt13-present-exclude-auto-crop.png
Measured samples / p50 / p95 / max where applicable:
  presentUs ~46-392 on overlay Present; CAPTUREBLT/PrintWindow magentaHits=3600; OBS positive magentaHits=2016 yellowHits=1378; OBS exclude magentaHits=0
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  DXGI_ALPHA_MODE_PREMULTIPLIED CreateSwapChainForHwnd failed 0x887A0001; overlay body is opaque black
  DXGI Desktop Duplication and Windows 10 (1903+) Display Capture methods NOT re-recorded after presenter change
  ordinary green ReferenceWindow, HWND recreate, opacity sweep, CSP resize, 60s duration NOT RUN in 13-present clips
  DISPLAY2 InputProbe click-through inconclusive (MozillaDialogClass); titled CSP HWND pass-through NOT RUN
  mixed-DPI still both 96; no negative-origin monitor; pen/pressure NOT RUN
  OBS websocket disabled; recording started by clicking Controls Start Recording
Manual checklist remaining:
  1. Optional later: retry DXGI DDA and Windows 10 (1903+) methods; restore per-pixel transparency without NOREDIRECTIONBITMAP/DComp.
  2. Prompt 14 — pure transform engine is unblocked for Automatic on this configuration.
Blocker or next prompt: 14 — Pure transform engine
```

```text
OS / GPU / driver / monitor / DPI / HDR:
  Windows 10 Pro 25H2 build 26200.9457 SDR (P709 G22); NVIDIA RTX 3060 32.0.16.1656; R27qe 2560x1440 @ (1920,0) 96 DPI 180Hz
OBS version / Display Capture method label / settings:
  OBS 32.2.2; Display Capture `Main Monitor`; method Automatic; display R27qe 2560x1440 @ 1920,0; cursor on; Force SDR off; 1920x1080 60fps HEVC
CSP version / app commit / renderer path:
  CLIPStudioPaint 5.0.0 pid 36888; TracingApp Debug dxgi-hwnd overlay (alphaMode=ignore) + skip-image-present; InputProbe covered at (2008,111)
Private HWND affinity set result / readback / error:
  positive-control set=ok win32=0 readback=0x00000000 matchNone=yes
Positive control: PASS
Both markers physically visible: magenta PASS (CAPTUREBLT/PrintWindow/OBS playback); green ReferenceWindow NOT RUN this clip
Saved recording playback: PASS (magenta+yellow L present)
Private marker absent, including transitions: n/a (positive control)
Underlying CSP intact (no black replacement): n/a (positive control shows opaque overlay body by design of ALPHA_IGNORE)
Ordinary reference visible: NOT RUN this clip
Recreated HWND / moved monitor retest: not in this clip
Recording path / inspected timestamps:
  I:/Videos/Obs/GD/2026-09-25 21-40-35.mp4 duration 9.932s; still ~t=2s plus magenta crop
Tested by / date: 13-present 2026-09-25
Supported only for this configuration: YES (Automatic + dxgi-hwnd ignore-alpha on this OS/GPU/monitor)
```

```text
OS / GPU / driver / monitor / DPI / HDR:
  Windows 10 Pro 25H2 build 26200.9457 SDR (P709 G22); NVIDIA RTX 3060 32.0.16.1656; R27qe 2560x1440 @ (1920,0) 96 DPI 180Hz
OBS version / Display Capture method label / settings:
  OBS 32.2.2; Display Capture `Main Monitor`; method Automatic; display R27qe 2560x1440 @ 1920,0; cursor on; Force SDR off; 1920x1080 60fps HEVC
CSP version / app commit / renderer path:
  CLIPStudioPaint 5.0.0 pid 36888; TracingApp Debug dxgi-hwnd overlay (alphaMode=ignore) + skip-image-present; overlay still covering InputProbe
Private HWND affinity set result / readback / error:
  set=ok win32=0 readback=0x00000011 matchExclude=yes
Positive control: PASS (prior Automatic clip)
Both markers physically visible: overlay HWND visible=yes during recording (app); magenta independent of OBS via CAPTUREBLT
Saved recording playback: PASS for exclusion
Private marker absent, including transitions: PASS (magenta=0 yellow=0 in t~3s still)
Underlying CSP intact (no black replacement): PASS (InputProbe teal client and CSP toolbar visible; not a black hole)
Ordinary reference visible: NOT RUN this clip
Recreated HWND / moved monitor retest: not in this clip
Recording path / inspected timestamps:
  I:/Videos/Obs/GD/2026-09-25 21-45-38.mp4 duration 11.932s; still ~t=3s plus crop
Tested by / date: 13-present 2026-09-25
Supported only for this configuration: YES (Automatic + dxgi-hwnd ignore-alpha on this OS/GPU/monitor)
```

```text
Step / milestone: 14 / M5 numerical transform engine (not visual calibration)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 9fc0b60 (uncommitted)
Status: PASS for Debug configure/build/test and canned numerical diagnostic. Visual landmark alignment, overlay clipping, and connecting M_RD to ImageRenderer remain Prompt 15. M5 overall NOT RUN.
Changed files:
  src/core/Transform2D.h (new)
  src/core/Transform2D.cpp (new)
  tests/unit/TransformTests.cpp (new)
  CMakeLists.txt
  src/app/main.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2284032 bytes (2026-09-25 22:03:29)
  TracingApp.TransformTests.exe 925696 bytes (2026-09-25 22:03:00)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 8, passed 8, failed 0 (new TracingApp.TransformTests)
  TracingApp.TransformTests --gtest_list_tests: 15 cases (known points, noncommuting order, off-center pivots, both flip axes, FlipX*FlipY vs Rotate(pi), negative-origin M_SO, inverse round-trip, finite/singular, space mismatch, full R-D-S-O chain, canned diagnostic text)
Manual setup and exact actions:
  Start-Process out/build/windows-debug/Debug/TracingApp.exe (PID 29824, title=TracingApp). Status before click contained "Transform diag:" pointer and did not contain actual pO=. SendMessage BM_CLICK to button id 1021. Read STATIC id 1004. CloseMainWindow; WaitForExit.
Expected / actual:
  expected: canned identity M_RD, M_DS=T(100,200)*S(2), M_SO origin=(-1920,108) maps pR=(10,5) to pO=(2040,102); inverse round-trip ~0; not wired to image placement
  actual: status after click: actual pO=(2040,102); inverse round-trip |pR'-pR|=0.000e+00; M_RD/M_DS/M_SO finite=yes singular=no; CloseMainWindow=True ExitCode=0 leftover=0
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.TransformTests.exe
  out/manual/prompt14-transform-diag.txt
Measured samples / p50 / p95 / max where applicable:
  inverse round-trip 0.000e+00 on canned fixture; no registration-error samples (not tracking)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  negative-origin monitor still absent; canned M_SO origin=(-1920,108) is the numerical stand-in
  ImageRenderer still uses explicit ImagePlacement; matrices are not uploaded to HLSL
  canonical 3x3 storage does not remove measurement drift
  CSP/OBS/pen not exercised this step
Manual checklist remaining:
  1. Prompt 15 — manual calibration, viewport clipping, and visual landmark alignment.
  2. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency.
Blocker or next prompt: 15 — Manual calibration and viewport clipping
```

```text
Step / milestone: 15a / M5 manual calibration engine + ROI HWND clip (not visual rotation/flip)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 2c3e24f (uncommitted)
Status: PASS for Debug configure/build/test and control-window calibration smoke. CSP landmark/move/mixed-DPI NOT RUN. ImageRenderer affine rotation/flip remains substep B. M5 overall PARTIAL.
Changed files:
  src/core/Calibration.h (new)
  src/core/Calibration.cpp (new)
  tests/unit/CalibrationTests.cpp (new)
  CMakeLists.txt
  src/app/main.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2323456 bytes (2026-09-25 22:33:14)
  TracingApp.CalibrationTests.exe 982016 bytes (2026-09-25 22:33:18)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 9, passed 9, failed 0 (new TracingApp.CalibrationTests)
  TracingApp.CalibrationTests --gtest_list_tests: 15 cases (ROI accept/reject, capture-to-screen with negative origin, unmatched mapping, local vs declared document units, window-move M_RD stability, size/generation invalidation, mapping-required reject, landmark clip after origin change, axis-aligned placement vs rotation reject, fit contain, report labels)
Manual setup and exact actions:
  Get-CimInstance Win32_Process filtered clipstudio|CLIPStudio: no matching processes
  Start-Process out/build/windows-debug/Debug/TracingApp.exe (PID 15464, class TracingAppControlWindow). Read STATIC id 1004. SendMessage BM_CLICK Apply ROI id 1026. BM_CLICK Transform diag id 1021. CloseMainWindow; WaitForExit.
Expected / actual:
  expected: status contains tracking=disabled and local calibrated units; Apply ROI without a target refuses; canned diag still maps pO=(2040,102); clean exit
  actual: before click status included tracking=disabled, roiApplied=no, units=local calibrated units (not verified document pixels or CSP zoom %), M_RD/M_DS listed separately; after Apply ROI: "Apply ROI: select a PAINT HWND first."; after Transform diag: actual pO=(2040,102) inverse round-trip 0.000e+00; CloseMainWindow ExitCode=0 leftover=0
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.CalibrationTests.exe
  out/manual/prompt15-control-smoke.txt
Measured samples / p50 / p95 / max where applicable:
  no CSP landmark registration samples (CLIPStudioPaint not running)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  CLIPStudioPaint not running; mixed-DPI and negative-origin monitors still absent
  overlay HWND clip covers the ROI with opaque ALPHA_IGNORE black outside the image
  rotation/flip stored in M_RD/M_DS but ImagePlacement is translation+uniform scale only
  automatic tracking remains disabled
Manual checklist remaining:
  1. Prompt 15 substep B — ImageRenderer affine (rotation/flip) plus RS scissor; CSP landmark pan/zoom/rotation/flip and same-DPI window move.
  2. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency.
Blocker or next prompt: 15b — ImageRenderer affine placement and CSP visual calibration, then 16
```

```text
Step / milestone: 15b / M5 ImageRenderer affine + RS scissor (CSP eyeball landmarks still NOT RUN)
Date / commit: 2026-09-25 / working tree on feature/init ahead of fc37974 (uncommitted)
Status: PASS for Debug configure/build/test, M_RO affine upload, RS scissor, titled-CSP ROI HWND clip, and same-DPI window-move with M_RD unchanged. Eyeballed asymmetric landmark pixels NOT RUN (CopyFromScreen omits WDA_EXCLUDEFROMCAPTURE). Mixed-DPI NOT RUN. M5 overall PARTIAL.
Changed files:
  shaders/Image.hlsl
  src/graphics/ImageRenderer.h
  src/graphics/ImageRenderer.cpp
  src/app/main.cpp
  src/core/Calibration.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  shaders/Image.hlsl compiled to ImageVS.cso / ImagePS.cso
  TracingApp.exe 2323456 bytes (2026-09-25 22:54:00)
  TracingApp.CalibrationTests.exe 982016 bytes (2026-09-25 22:54:03)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 9, passed 9, failed 0
  TracingApp.CalibrationTests --gtest_list_tests: 15 cases (axis-aligned helper still rejects rotation; that is CPU policy, not the renderer)
Manual setup and exact actions:
  CLIPStudioPaint.exe pid 36888 was running. Generated local 96x48 magenta-L + yellow-block PNG at out/manual/prompt15b-asymmetric.png (not committed). Start-Process TracingApp.exe pid 45296. Selected largest PAINT row: [PAINT] PID 36888 CLIPStudioPaint.exe | CLIP STUDIO PAINT, client physical origin=(1920,0) size=2576x1440 on DISPLAY1/R27qe. Apply ROI 80,80 640x480. IFileOpenDialog imported the PNG (uploaded generation=1). Clicked M_RD R+ and FX, M_DS R+ and FY. SetWindowPos titled CSP +80,+40, waited 600 ms, restored original position. CloseMainWindow timed out; process killed; leftover then cleared.
Expected / actual:
  expected: overlay HWND at client+ROI; clipO overlay-local (0,0,640,480); image constants include M_RO 2x3; rotation/flip make axisAlignedPlacement=no; window move changes overlay origin / M_SO not M_RD; ReferenceWindow stays independent axis-aligned fit
  actual: overlay placement origin=(2000,80) size=640x480 clipO=(0,0,640,480) scissor=overlay-rt; after import Fit used M_RD offset=(0,80) scale=6.66667; after rot/flip M_RD rad=0.26180 flipX=yes and M_DS rad=0.26180 flipY=yes, renderer affine=[[-6.66667,0.00000,20.71],[0.00000,-6.66667,-77.27]] axisAlignedPlacement=no (rotation/flip drawn via overlay-local affine; HWND+RS scissor); combined flips produced diagonal negative scale (off-diagonals cancelled). After CSP move client origin=(2000,40) overlay origin=(2080,120) dX=80 dY=40 geomGen 1->2, M_RD unchanged. CopyFromScreen of overlay rect showed CSP chrome (color wheel / tool group), not the private PNG. ReferenceWindow independentFit scale=6.50000 textureGeneration=1.
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.CalibrationTests.exe
  out/manual/prompt15b-asymmetric.png
  out/manual/prompt15b-overlay.png
  out/manual/prompt15b-session.txt
Measured samples / p50 / p95 / max where applicable:
  no landmark registration-error samples (no eyeballed pixel ground truth)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  mixed-DPI and negative-origin monitors still absent (both displays 96 DPI)
  ALPHA_IGNORE overlay body remains opaque black outside the image
  WDA_EXCLUDEFROMCAPTURE prevents CopyFromScreen from capturing overlay pixels
  CloseMainWindow did not exit within 5 s (reference HWND still visible); process was killed
  automatic tracking remains disabled
Manual checklist remaining:
  1. Eyeball asymmetric landmarks on the physical display (pan/zoom/rotation/flip vs CSP canvas).
  2. Prompt 16 — bounded ROI readback.
  3. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency.
Blocker or next prompt: 16 — Bounded ROI readback
```

```text
Step / milestone: 16a / M6 bounded ROI readback (default full-content canvas ROI; applied client ROI is 16b)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 2da36f1 (uncommitted)
Status: PASS for Debug configure/build/test, GPU-free clip/downsample/stride/pending tests, control-window roi (none) smoke, and FormatReport zero-copy disclaimer. Live CSP copyUs/mapWaitUs/pending/resize NOT RUN. Applied canvas/Navigator ROI from calibration not wired (CaptureSession.h + main.cpp would exceed five-file scope).
Changed files:
  src/capture/RoiReadback.h (new)
  src/capture/RoiReadback.cpp (new)
  src/capture/CaptureSession.cpp
  CMakeLists.txt
  tests/unit/RoiTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2364416 bytes (2026-09-25 23:14:12)
  TracingApp.RoiTests.exe 975872 bytes (2026-09-25 23:14:16)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 10, passed 10, failed 0 (new TracingApp.RoiTests)
  TracingApp.RoiTests --gtest_list_tests: 12 cases (canvas interior, navigator clip, non-positive/fully-outside reject, partial OOB clip, downsample 2 and odd floor, byte-limit/oversized-axis, default downsample 640/1025/2576, pitch-256 pack of known pixel, point-sample D=2, pending bound drops oldest metadata, DiscardAll, zeroCopy=no label)
Manual setup and exact actions:
  1. Control smoke: Start-Process out/build/windows-debug/Debug/TracingApp.exe PID 42420 class TracingAppControlWindow. Read STATIC id 1004. CloseMainWindow; WaitForExit.
  2. CSP: Get-CimInstance Win32_Process clipstudio|CLIPStudio found none. CLIPStudioPaint.exe exists at C:\Program Files\CELSYS\CLIP STUDIO 1.5\CLIP STUDIO PAINT\CLIPStudioPaint.exe. Start-Process was canceled by the user; no titled CLIP STUDIO PAINT window. Did not invent copy/map waits.
Expected / actual:
  expected: status includes roi line and packed-copy disclaimer; no WGC session until a HWND is selected; tests pass
  actual: status line "roi (none) (WGC copies owned textures; ROI CPU buffer is a packed copy, not zero-copy)"; CloseMainWindow ExitCode=0 leftover=0
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.RoiTests.exe
  out/manual/prompt16-control-smoke.txt
Measured samples / p50 / p95 / max where applicable:
  live CSP copyUs / mapWaitUs / pending / recreates: NOT RUN (no titled CSP HWND)
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  default live ROI is owned capture content with D=ceil(max(w,h)/1024), not the applied client-relative canvas ROI
  Navigator slot exists in clip policy only; no Navigator UI (prompt 20)
  CPU OpenCV path is not zero-copy (packed BGRA rows after Map)
  mixed-DPI and negative-origin monitors still absent
  automatic tracking remains disabled
Manual checklist remaining:
  1. Prompt 16b — pass applied client-relative canvas ROI through measured capture-to-client mapping (CaptureSession.h + main.cpp); measure copyUs/mapWaitUs/pending<=2 on titled CSP including resize.
  2. Prompt 17 — OpenCV visual estimator (after 16b, or after documenting that 16b stays a follow-up).
  3. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 16b — applied canvas ROI wiring, then 17 — OpenCV visual estimator
```

```text
Step / milestone: 16b / M6 applied canvas ROI wiring (client-relative ROI through capture-to-client mapping)
Date / commit: 2026-09-25 / working tree on feature/init ahead of 141bf32 (uncommitted)
Status: PASS for Debug configure/build/test, client->capture mapping tests, live titled-CSP applied-mapped 640x480 readback (copyUs/mapWaitUs/pending<=2), and resize invalidation fallback. Re-apply after resize set roiApplied=yes but the next mapped copy was not observed within 800 ms of idle WGC. Navigator still unwired.
Changed files:
  src/core/Calibration.h
  tests/unit/CalibrationTests.cpp
  src/capture/CaptureSession.h
  src/capture/CaptureSession.cpp
  src/app/main.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2370048 bytes (2026-09-25 23:28:32)
  TracingApp.CalibrationTests.exe 1005056 bytes (2026-09-25 23:28:36)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 10, passed 10, failed 0
  TracingApp.CalibrationTests --gtest_list_tests: 17 cases (new ClientRoiToCaptureUsesMeasuredOffset 80,80,640,480 + offset 8,31 -> 88,111,640,480; inverse round-trip of capture (10,20); unvalidated/non-positive reject)
Manual setup and exact actions:
  CLIPStudioPaint.exe pid 36888 running. Start-Process TracingApp.exe pid 39224 class TracingAppControlWindow. Selected [PAINT] PID 36888 | CLIP STUDIO PAINT. Start Capture. Idle WGC stayed at lastSeq=1 (full-content-fallback, pending=1, hasCpu=no) until further target activity. Apply ROI 80,80 640x480 (defaults). After later owned frames: lastSeq=2 then 4, roiSrc=applied-mapped, clipped=80,80 640x480 out=640x480 ds=1 usingOffset=(0,0) on the restored HWND (earlier start-capture snapshot on the same session before restore had usingOffset=(8,8) match=outer-and-dwm). SetWindowPos titled CSP +160x+100 from (1765,192) 2576x1440. CloseMainWindow timed out; process killed; leftover then cleared. CSP HWND restored to original outer size.
Expected / actual:
  expected: applied client ROI maps through capture-to-client; ds=1 for 640x480; pending<=2; resize invalidates then full-content fallback; packed copy not zero-copy
  actual: applied-mapped clipped=80,80 640x480 ds=1 copyUs p50=3us p95=3 max=3 (n=11) mapWaitUs p50=0 p95=0 max=1 pending=1 hasCpu=yes zeroCopy=no dropped=1 (staging recreate when switching from full-content 2576x1440 to 640x480). Resize: invalidation=client-size-changed roiSrc=full-content-fallback clipped=0,0 2736x1460 pending=1 recreates=3 lastSeq=8. Re-apply: roiApplied=yes invalidation=none; roiSrc still full-content-fallback at lastSeq=8 after 800 ms idle (mapped clip updates on the next owned frame).
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.CalibrationTests.exe
  out/manual/prompt16b-session.txt
Measured samples / p50 / p95 / max where applicable:
  applied-mapped copyUs n=11 p50=3us p95=3 max=3
  applied-mapped mapWaitUs n=11 p50=0us p95=0 max=1
  applied-mapped pending max=1 (bound 2)
  resize fallback copyUs n=12 p50=3us p95=3 max=3; mapWaitUs p50=1 max=1; pending max=1; lastSeq max=8
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  idle titled CSP delivered sparse WGC frames (lastSeq stayed 1 until window activity); first owned copy leaves pending=1 hasCpu=no until a later TryComplete
  usingOffset was (8,8) on the first start-capture geometry and (0,0) after ShowWindow restore; mapped clip matched 80+offset
  Navigator slot exists in clip policy only; no Navigator UI (prompt 20)
  CPU OpenCV path is not zero-copy (packed BGRA rows after Map)
  mixed-DPI and negative-origin monitors still absent
  automatic tracking remains disabled
  CloseMainWindow did not exit within 4 s (reference HWND still visible); process was killed
Manual checklist remaining:
  1. Prompt 17 — OpenCV visual estimator.
  2. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 17 — OpenCV visual estimator
```

```text
Step / milestone: 17 / M6 OpenCV visual estimator (canvas vs prior/keyframe pixels)
Date / commit: 2026-09-25 / working tree on feature/init HEAD b2acb34 (uncommitted Prompt 17 files)
Status: PASS for Debug configure/build/test and 21 synthetic VisualTracker cases. Live CSP tracking NOT RUN (estimator not linked into TracingApp).
Changed files:
  vcpkg.json
  CMakeLists.txt
  src/tracking/VisualTracker.h (new)
  src/tracking/VisualTracker.cpp (new)
  tests/unit/VisualTrackerTests.cpp (new)
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  installed opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 (4.2 min) plus zlib 1.3.2#1
  Found OpenCV 4.12.0 components: core imgproc features2d calib3d
  TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  reused gtest:x64-windows@1.17.0#3; SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe unchanged 2370048 bytes (2026-09-25 23:28:32) — OpenCV not linked into the app
  TracingApp.VisualTrackerTests.exe 388608 bytes (2026-09-25 23:47:28)
  applocal copied opencv_core4d / imgproc4d / features2d4d / flann4d / calib3d4d + zd.dll
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 11, passed 11, failed 0 (new TracingApp.VisualTrackerTests 1.50s)
  TracingApp.VisualTrackerTests --gtest_list_tests: 21 cases; --gtest_brief=0: 21 PASSED
Manual setup and exact actions:
  none. Estimator is independent of rendering/input; no CSP session this step.
Expected / actual:
  expected: similarity estimate from canvas/keyframe pixels; reject blank, shear, implausible, and reflection without treating partial-affine as a flip
  actual: correspondence identity-grade errors (~0); textured 320x240 image path accepted; flips rejected reflection-unsupported
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.VisualTrackerTests.exe
  out/build/windows-debug/vcpkg-manifest-install.log
Measured samples / p50 / p95 / max where applicable:
  synthetic textured image errors (not CSP, not MASTER acceptance):
    img-translation txErr=0.015 tyErr=0.097 scaleErr=0.00030 degErr=0.029 inliers=394/419 rms=0.886 conf=0.607
    img-scale txErr=0.068 tyErr=0.174 scaleErr=0.00052 degErr=0.035 inliers=190/224 rms=1.297 conf=0.408
    img-rotate txErr=0.051 tyErr=0.027 scaleErr=0.00042 degErr=0.006 inliers=277/298 rms=1.177 conf=0.492
    img-combined txErr=0.017 tyErr=0.072 scaleErr=0.00047 degErr=0.045 inliers=187/218 rms=1.205 conf=0.444
    img-stroke txErr=0.151 tyErr=0.099 scaleErr=0.00013 degErr=0.021 inliers=254/280 rms=0.857 conf=0.596
    img-bgra txErr=0.043 tyErr=0.048 scaleErr=0.00015 degErr=0.014 inliers=406/432 rms=0.895 conf=0.603
    img-flip reject=reflection-unsupported conf=0 det=-1.045
    img-flip-y reject=reflection-unsupported conf=0 det=-1.076
  correspondence translation/scale/rotate residuals at numerical noise; 8 injected outliers kept 70/78 inliers
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  VisualTracker is not linked into TracingApp; automatic tracking remains disabled
  reflection hypotheses deferred to prompt 19; this step rejects det<0 rather than recovering flips
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP (no live tracking this step)
  mixed-DPI and negative-origin monitors still absent
Manual checklist remaining:
  1. Prompt 18 — TrackingSession + ROI buffer wiring + live CSP pan/zoom/rotation.
  2. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 18 — Tracking state and baseline integration
```

```text
Step / milestone: 18a / M6 tracking state machine (ROI feed deferred)
Date / commit: 2026-09-26 / working tree on feature/init HEAD bc11092 (uncommitted Prompt 18a files)
Status: PASS for Debug configure/build/test and 11 TrackingSession GoogleTests. Live CSP pan/zoom/rotation, blank canvas, and minimize/restore NOT RUN (CaptureSession LastRoiBuffer not exposed).
Changed files:
  src/tracking/TrackingSession.h (new)
  src/tracking/TrackingSession.cpp (new)
  tests/unit/TrackingSessionTests.cpp (new)
  CMakeLists.txt
  src/app/main.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 and gtest:x64-windows@1.17.0#3
  Found OpenCV 4.12.0; TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2510336 bytes (2026-09-26 00:04:55) — now links VisualTracker + TrackingSession + OpenCV
  TracingApp.TrackingSessionTests.exe 427008 bytes (2026-09-26 00:05:44)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 12, passed 12, failed 0 (new TracingApp.TrackingSessionTests 0.03s)
  TracingApp.TrackingSessionTests --gtest_list_tests: 11 cases (2 policy, 8 session, 1 similarity)
  first ctest after the initial build: TrackingSessionTests FAILED 2/11 on EXPECT_EQ of const char* pointers; rebuilt with EXPECT_STREQ; rerun 11/11 PASSED
Manual setup and exact actions:
  Start-Process out/build/windows-debug/Debug/TracingApp.exe PID 12116 MainWindowTitle=TracingApp Responding=True. CloseMainWindow=True WaitForExit=True leftover=0.
  Did not select CSP or Start tracking against a titled painting window.
Expected / actual:
  expected: TrackingSession states, 150/300 ms age, hide-on-Lost, generation/order/contradiction/reacquire tests; app status reports tracking line; Start/Pause/Resync present
  actual: tests PASS; control window launched and exited cleanly; live registration quality NOT MEASURED
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.TrackingSessionTests.exe
Measured samples / p50 / p95 / max where applicable:
  live CSP confidence / residual / pan-zoom-rotation error: NOT RUN
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  CaptureSession does not expose LastRoiBuffer; worker has no live canvas pixels
  Start tracking stays Calibrating until 18b submits ROI
  hideOverlay uses existing targetUsable=false; no new OverlayVisibilityReason
  reflection hypotheses deferred to prompt 19
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP
  mixed-DPI and negative-origin monitors still absent
Manual checklist remaining:
  1. Prompt 18b — expose CaptureSession LastRoiBuffer, feed TrackingSession, apply snapshot M_DS, live CSP pan/zoom/rotation / blank / minimize.
  2. Optional later: DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 18b — ROI buffer wiring and live CSP tracking (do not auto-run)
```

```text
Step / milestone: 18b / M6 ROI feed + live tracking
Date / commit: 2026-09-26 / working tree on feature/init HEAD bc11092 (uncommitted Prompt 18a+18b files)
Status: PASS for Debug configure/build/test and worker-path GoogleTests. Live titled-CSP ROI CPU feed and hide-on-Lost PASS. Settled live Tracking / pan-zoom lock NOT RUN as a pass — default ROI lost tracking (poor-coverage and/or 300 ms age). Blank canvas NOT RUN. Import/reference overlay follow NOT RUN (IFileOpenDialog).
Changed files:
  src/capture/CaptureSession.h
  src/capture/CaptureSession.cpp
  src/app/main.cpp
  src/tracking/TrackingSession.cpp
  tests/unit/TrackingSessionTests.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 and gtest:x64-windows@1.17.0#3
  Found OpenCV 4.12.0; TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2520576 bytes (2026-09-26 00:31:46)
  TracingApp.TrackingSessionTests.exe 495104 bytes (2026-09-26 00:31:49)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 12, passed 12, failed 0
  TracingApp.TrackingSessionTests --gtest_list_tests: 13 cases (2 policy, 10 session including WorkerTexturedTranslationMovesSnapshot and WorkerBlankAfterKeyframeDoesNotJump, 1 similarity)
  TracingApp.TrackingSessionTests 0.34s
Manual setup and exact actions:
  CLIPStudioPaint.exe pid 36888 titled HWND CLIP STUDIO PAINT client 2560x1439 at (1920,0) DISPLAY1/R27qe. Debug TracingApp.exe. Selected largest PAINT row. Start Capture, Apply ROI 80,80 640x480, Start tracking. Space-drag pan and Ctrl-wheel zoom sent to CSP (no document-clear). Minimize then restore titled HWND. Logs: out/manual/prompt18b-live.txt (first, LastRoiBuffer lagged), prompt18b-live-2.txt (feed seq 42 inliers=683 conf=0.333 then Lost), prompt18b-live-3.txt (keyframe seq=26 then poor-coverage Lost). IFileOpenDialog import not driven. CloseMainWindow often needed kill after 4–6s; leftover then 0.
Expected / actual:
  expected: ROI CPU buffers submitted; snapshot M_DS used while attached; hide on Lost; worker tests; live pan/zoom/rotation quality recorded honestly
  actual: tests PASS. Live-2: roiSrc=applied-mapped hasCpu=yes roiFeed enqueue/drop-oldest, keyframe=yes inliers=683 conf=0.333 reject=ok, then state=Lost hide=yes ageMs=2475 overlay Hide target-unusable; WGC lastSeq kept rising. Live-3: keyframe conf=1.000 then reject=poor-coverage Lost. Minimize: roiApplied=no tracking Unattached eligibility=minimized client origin=(-32000,-32000); restore eligibility=eligible but calibration still invalid (needs re-Apply ROI + Start tracking). Overlay did not remain locked through pan/zoom. MASTER 2 px / 0.5 deg not claimed.
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.TrackingSessionTests.exe
  out/manual/prompt18b-live.txt
  out/manual/prompt18b-live-2.txt
  out/manual/prompt18b-live-3.txt
Measured samples / p50 / p95 / max where applicable:
  worker test translation: expected +8,+5 px at document origin, assert tolerance 2.5 px
  live CSP registration error: NOT MEASURED (no landmark ground truth; tracking Lost before a settled lock)
  live-2 copyUs~2–4 us mapWaitUs~1–2 pending=2 mapNotReady increased with WGC rate
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  default 80,80 640x480 ROI on this workspace may include non-canvas chrome; poor-coverage is a valid reject
  Tick no longer applies 150/300 ms age while a ROI frame is policy-pending (ORB can exceed 300 ms); age still applies between estimates
  imported reference overlay follow NOT RUN (IFileOpenDialog)
  live blank canvas NOT RUN (open document not cleared)
  space-pan / ctrl-wheel / shift-space-rotate were sent; CSP gesture mapping not verified; tracking already Lost
  reflection hypotheses deferred to prompt 19
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP
  mixed-DPI and negative-origin monitors still absent
Manual checklist remaining:
  1. Prompt 19 — reflection hypotheses and relocalization.
  2. Optional later: retune live ROI onto a textured canvas region; DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 19 — Reflection hypotheses and relocalization (do not auto-run)
```

```text
Step / milestone: 19a / M6 reflection hypotheses (session relocalization tests deferred)
Date / commit: 2026-09-26 / working tree on feature/init HEAD c25427c (uncommitted Prompt 18a+18b+19a files)
Status: PASS for Debug configure/build/test and 28 VisualTracker GoogleTests (flip recovery, two-flip canonical 180, symmetric ambiguity, prolonged outliers, return-to-keyframe). Live CSP flip/parity NOT RUN. TrackingSessionTests unchanged 13 cases still PASS.
Changed files:
  src/tracking/VisualTracker.h
  src/tracking/VisualTracker.cpp
  src/tracking/TrackingSession.h
  src/tracking/TrackingSession.cpp
  tests/unit/VisualTrackerTests.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 and gtest:x64-windows@1.17.0#3
  Found OpenCV 4.12.0; TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2520576 bytes (2026-09-26 08:45:49)
  TracingApp.VisualTrackerTests.exe 481792 bytes (2026-09-26 08:45:28)
  TracingApp.TrackingSessionTests.exe 504832 bytes (2026-09-26 08:45:52)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 12, passed 12, failed 0
  TracingApp.VisualTrackerTests --gtest_list_tests: 28 cases (adds CorrespondenceHorizontalFlipAccepted, CorrespondenceVerticalFlipAccepted, ImageHorizontalFlipAccepted, ImageVerticalFlipAccepted, ImageFlipXWithOffCenterRotation, ImageFlipYWithTranslation, TwoFlipsCanonicalizedTo180Rotation, SymmetricSceneParityAmbiguous, ProlongedStrokeOutliersKeepParity, ReturnToKeyframeIsIdentity)
  TracingApp.VisualTrackerTests 6.61s
  TracingApp.TrackingSessionTests 0.64s (13 cases)
Manual setup and exact actions:
  CLIPStudioPaint.exe pid 11744 was running with empty MainWindowTitle (not treated as a titled painting HWND). Did not Start Capture / Apply ROI / Start tracking or invoke CSP flip-horizontal / flip-vertical.
  Exact steps if a titled CLIP STUDIO PAINT document is available: select the PAINT row; Start Capture; Apply ROI onto an asymmetric drawing (not a blank or left-right-symmetric canvas); Start tracking; wait for state=Tracking with parity=none; use CSP Flip Horizontal then Flip Vertical; watch status parity= and driftRms= vs reacquire=; if status becomes Paused with reject=parity-ambiguous, Resync rather than treating alignment as success. Close TracingApp with CloseMainWindow (18b often needed kill after 4-6s).
Expected / actual:
  expected: remapped FlipX/FlipY hypotheses recover orientation-preserving fits; FlipX*FlipY reports no flip flags and ~180 deg; symmetric identity is ambiguous not a guessed flip; origin keyframe is not replaced; snapshot reports parity and separate drift vs reacquire; live CSP flips recorded if hardware allows
  actual: tests PASS. img-flip-x maxMapErr=0.048 conf=0.631; img-flip-y maxMapErr=0.445 conf=0.503; img-flip-x-rot maxMapErr=0.171; img-flip-y-pan maxMapErr=0.450; img-two-flips maxMapErr=0.393 conf=0.485 |ang|~180 parity=none; sym reject=parity-ambiguous conf=0; prolonged stroke four frames stay translation ~8,5 no flip; return-to-keyframe identity conf=1. Live CSP flips NOT RUN. Origin keyframe still only captured once per BeginCalibrated/Resync (no unreliable promotion).
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.VisualTrackerTests.exe
  out/build/windows-debug/Debug/TracingApp.TrackingSessionTests.exe
Measured samples / p50 / p95 / max where applicable:
  synthetic 320x240 flip mapping maxErr as printed by VisualTrackerTests (see actual above); not MASTER 2 px / 0.5 deg claims
  live CSP flip residual / reacquire: NOT RUN
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  CLIPStudioPaint was running but untitled; 18b default ROI still does not hold settled Tracking
  TrackingSessionTests do not yet cover Pause-on-ParityAmbiguous, parity hysteresis across SubmitEstimateForTest, or a second validated anchor
  FlipX+180 is canonicalized to FlipY (same physical transform); UI flip flags are not uniquely recoverable from pixels
  3x ORB hypotheses make VisualTrackerTests ~6.6s; live ORB latency was already a 18b Lost-age factor
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP
  mixed-DPI and negative-origin monitors still absent
Manual checklist remaining:
  1. Prompt 19b — TrackingSessionTests for hysteresis, Pause on ParityAmbiguous, return-to-keyframe at session level, optional second validated anchor, drift vs reacquire counters.
  2. Optional later: live CSP flip on an asymmetric drawing after ROI retune; DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 19b — TrackingSession relocalization / hysteresis tests (do not auto-run)
```

```text
Step / milestone: 19b / M6 TrackingSession relocalization and hysteresis
Date / commit: 2026-09-26 / working tree on feature/init HEAD 0f90b5b (uncommitted Prompt 19b files)
Status: PASS for Debug configure/build/test and 19 TrackingSession GoogleTests (Pause-on-ParityAmbiguous, parity hysteresis, worker return-to-keyframe, secondary not promoted on blank, secondary relocalize when origin residual worse, reacquire events separate from drift). Live CSP flip/parity NOT RUN (CLIPStudioPaint pid 11744 empty MainWindowTitle).
Changed files:
  src/tracking/TrackingSession.h
  src/tracking/TrackingSession.cpp
  tests/unit/TrackingSessionTests.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 and gtest:x64-windows@1.17.0#3
  Found OpenCV 4.12.0; TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2545664 bytes (2026-09-26 09:06:11)
  TracingApp.TrackingSessionTests.exe 656896 bytes (2026-09-26 09:06:30)
  TracingApp.VisualTrackerTests.exe 481792 bytes (2026-09-26 09:06:17)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 12, passed 12, failed 0
  TracingApp.TrackingSessionTests --gtest_list_tests: 19 cases (adds ParityAmbiguousPausesAndDoesNotMoveTransform, ParityHysteresisRejectsSingleFlipThenAccepts, WorkerReturnToKeyframeRestoresIdentity, SecondaryAnchorNotPromotedOnBlank, RelocalizeUsesSecondaryWhenOriginResidualWorse, ReacquireEventsSeparateFromDrift)
  TracingApp.TrackingSessionTests 1.83s
  TracingApp.VisualTrackerTests 6.41s (28 cases, unchanged)
Manual setup and exact actions:
  CLIPStudioPaint.exe pid 11744 was running with empty MainWindowTitle (not treated as a titled painting HWND). CLIPStudio launcher pid 47232 titled CLIP STUDIO. Did not Start Capture / Apply ROI / Start tracking or invoke CSP flip-horizontal / flip-vertical.
  Exact steps if a titled CLIP STUDIO PAINT document is available: select the PAINT row; Start Capture; Apply ROI onto an asymmetric drawing (not a blank or left-right-symmetric canvas); Start tracking; wait for state=Tracking with parity=none; watch secondary=yes after a quality non-identity observation; use CSP Flip Horizontal then Flip Vertical; watch status parity= and driftRms= vs reacquire=; a single noisy opposite-parity frame should not flip; if status becomes Paused with reject=parity-ambiguous, Resync rather than treating alignment as success. Close TracingApp with CloseMainWindow (18b often needed kill after 4-6s).
Expected / actual:
  expected: ParityAmbiguous pauses without moving M_DS; two consecutive FlipX estimates pass hysteresis; worker return to origin keyframe restores identity; blank does not promote a second anchor; high origin residual with a good secondary relocates using the composed secondary pose while driftRms stays the origin residual; successful Lost reacquire increments a cumulative counter that blank rejects do not; live CSP flips recorded if hardware allows
  actual: tests PASS. ParityAmbiguous -> Paused hide=no sequence unchanged; later Ok while Paused Ignored. One FlipX -> Degraded M_DS held; second FlipX -> Tracking flipX. Worker seq3 original pixels restored origin within 2.5 px and hasKeyframe=yes. Blank after keyframe hasSecondaryAnchor=no. Origin HighResidual 6.5 px + secondary +2 px accepted usedSecondary=yes driftRms=6.5 rms=0.35. Lost then 3x Ok reacquireCount>=1; blank leaves reacquireCount unchanged. Live CSP flips NOT RUN. Origin keyframe still only captured once per BeginCalibrated/Resync (secondary is additional, not a replacement).
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.TrackingSessionTests.exe
  out/build/windows-debug/Debug/TracingApp.VisualTrackerTests.exe
Measured samples / p50 / p95 / max where applicable:
  worker return-to-keyframe origin restore asserted at 2.5 px (same tolerance as 18b translation test); not MASTER 2 px / 0.5 deg claims
  live CSP flip residual / reacquire: NOT RUN
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  CLIPStudioPaint was running but untitled; 18b default ROI still does not hold settled Tracking
  FlipX+180 is canonicalized to FlipY (same physical transform); UI flip flags are not uniquely recoverable from pixels
  3x ORB hypotheses plus optional secondary estimate add worker latency; live ORB latency was already a 18b Lost-age factor
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP
  mixed-DPI and negative-origin monitors still absent
Manual checklist remaining:
  1. Prompt 20 — Navigator observer.
  2. Optional later: live CSP flip on an asymmetric drawing after ROI retune; DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 20 — Navigator observer (do not auto-run)
```

```text
Step / milestone: 20a / M7 Navigator observer (CPU geometry + UI; no WGC navigator buffer)
Date / commit: 2026-09-26 / working tree on feature/init HEAD 6967fb1 (uncommitted Prompt 20a files)
Status: PASS for Debug configure/build/test and 14 NavigatorObserver GoogleTests (13 run PASS, RealCspFixtureIsLocalOptIn SKIPPED). Live CSP Navigator inspect/pan/zoom/rotation/themes/hidden NOT RUN (CLIPStudioPaint pid 11744 empty MainWindowTitle). Fusion not connected.
Changed files:
  src/tracking/NavigatorObserver.h
  src/tracking/NavigatorObserver.cpp
  src/app/main.cpp
  CMakeLists.txt
  tests/unit/NavigatorTests.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 and gtest:x64-windows@1.17.0#3
  Found OpenCV 4.12.0; TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2609152 bytes (2026-09-26 09:40:16)
  TracingApp.NavigatorTests.exe 1081344 bytes (2026-09-26 09:41:16)
  TracingApp.BootstrapTests.exe 112640 bytes (2026-09-26 09:40:19)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 13, passed 13, failed 0
  TracingApp.NavigatorTests --gtest_list_tests: 14 cases (DisableIgnoresPixels, DefaultDisabledWithoutRoi, RejectsInvalidRoi, MissingOnBlankAndEmptyView, MissingOnTextureWithoutIndicator, PanChangesTranslationOnlyInThumbnail, RelativeScaleDoesNotClaimZoomPercent, RotationWhenUniqueNonSquare, OccludedWhenIndicatorHitsBorder, AmbiguousWhenTwoSimilarIndicators, MappingFillsDocumentNotUncalibratedInventedCoords, ReapplyRoiReenablesAfterDisable, FormatNamesAreStable, RealCspFixtureIsLocalOptIn)
  TracingApp.NavigatorTests 0.06s (1 skipped: TRACING_NAVIGATOR_FIXTURE unset)
  TracingApp.VisualTrackerTests 6.32s; TracingApp.TrackingSessionTests 1.71s (unchanged this step)
Manual setup and exact actions:
  CLIPStudioPaint.exe pid 11744 was running with empty MainWindowTitle (not treated as a titled painting HWND). CLIPStudio launcher pid 47232 titled CLIP STUDIO. Did not Start Capture / Apply Nav ROI on a live Navigator panel / hide Navigator / pan/zoom/rotate. Did not inspect actual Navigator thumbnail/indicator pixels this step; detector is the synthetic contrasting rectangle on a textured thumbnail.
  Exact steps if a titled CLIP STUDIO PAINT document with a visible Navigator is available: select the PAINT row; Start Capture; Apply ROI for canvas as usual; enter client-relative Navigator x y w h covering the thumbnail (not chrome); Apply Nav ROI (optional Doc px fills thumbnail-to-document mapping); confirm status nav src=missing navBuffer=no and that Start tracking / canvas roiFeed still work; Disable Nav and confirm tracking is unchanged; pan/zoom/rotate the canvas and hide Navigator — after 20b those should change measured u/v/wu/hv or src=missing without pausing visual tracking. Do not treat relativeZoom as CSP %. Close TracingApp with CloseMainWindow.
Expected / actual:
  expected: disabled/missing/occluded/ambiguous report no invented document coords or zoom%; pan moves thumbnail-normalized center; larger indicator raises relative scale without zoom% unless mapping applied; unique non-square reports rotation; mapping fills document X/Y from u/v * declared size and labels zoom as not CSP %; Disable ignores pixels; missing source does not Detach TrackingSession; live CSP comparison if hardware allows
  actual: tests PASS as listed. Uncalibrated observe without SetRoi is Disabled (source off until Apply Nav ROI). Format always includes "not CSP %". Live CSP Navigator NOT RUN. CaptureSession still has only SetCanvasRoiRequest / LastRoiBuffer; Observe(empty) after Apply Nav ROI yields missing/degenerate-size. TrackingSession.cpp was not modified.
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.NavigatorTests.exe
Measured samples / p50 / p95 / max where applicable:
  synthetic 160x120 indicator pan/scale/rotation asserted in NavigatorTests (center within 0.08 of known u; rotation delta < 0.20 rad); not MASTER 2 px / 0.5 deg claims
  live CSP Navigator residual / hide: NOT RUN
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  CLIPStudioPaint was running but untitled; Navigator thumbnail appearance on this CSP 5.0.0 theme was not inspected
  no WGC Navigator ROI path (CaptureSession canvas-only); listed as 20b
  rectangle geometry never reports parity; relativeZoom is not CSP percent
  opencv imgcodecs not linked; TRACING_NAVIGATOR_FIXTURE cannot be decoded this step
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP
  mixed-DPI and negative-origin monitors still absent
Manual checklist remaining:
  1. Prompt 20b — CaptureSession Navigator-kind ROI readback and Observe() feed.
  2. Optional later: live CSP Navigator pan/zoom/rotation/themes/hidden on a titled document; live CSP flip; DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 20b — CaptureSession Navigator ROI feed (do not auto-run Prompt 21)
```

```text
Step / milestone: 20b / M7 CaptureSession Navigator ROI feed
Date / commit: 2026-09-26 / working tree on feature/init HEAD 2f21f56 (uncommitted Prompt 20b files)
Status: PASS for Debug configure/build/test, 17 NavigatorObserver GoogleTests (16 run PASS, RealCspFixtureIsLocalOptIn SKIPPED), and control-window smoke (nav src=disabled navBuffer=no navRoiSrc=none). Live CSP Navigator inspect/pan/zoom/rotation/themes/hidden NOT RUN (no CLIPStudioPaint.exe or CLIPStudio.exe process). Fusion not connected.
Changed files:
  src/capture/CaptureSession.h
  src/capture/CaptureSession.cpp
  src/app/main.cpp
  tests/unit/NavigatorTests.cpp
  project-docs/VALIDATION.md
Configure command + exit code:
  $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
  cmake --preset windows-debug
  exit 0
  reused opencv4[calib3d,core,intrinsics,thread]:x64-windows@4.12.0#7 and gtest:x64-windows@1.17.0#3
  Found OpenCV 4.12.0; TracingApp fxc: C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
  SDK 10.0.26100.0; CXX MSVC 19.51.36260.0; binaryDir out/build/windows-debug
Build command + exit code:
  cmake --build --preset windows-debug
  exit 0
  MSBuild 18.10.1-1.26427.6+3cd27c13e
  TracingApp.exe 2620928 bytes (2026-09-26 09:57:51)
  TracingApp.NavigatorTests.exe 1131008 bytes (2026-09-26 09:57:59)
  TracingApp.BootstrapTests.exe 112640 bytes (2026-09-26 09:57:53)
Test command + discovered/passed/failed counts:
  ctest --preset windows-debug --output-on-failure
  exit 0
  discovered 13, passed 13, failed 0
  TracingApp.NavigatorTests --gtest_list_tests: 17 cases (prior 14 plus PackedBgraObserveCopiesMetadataWithoutZoomPercent, PackedBgraEmptyViewIsMissingWithoutDocumentCoords, DisableIgnoresPackedBgra)
  TracingApp.NavigatorTests 16 passed, 1 skipped (TRACING_NAVIGATOR_FIXTURE unset), 0.05s in ctest / 37 ms direct
  TracingApp.VisualTrackerTests 6.56s; TracingApp.TrackingSessionTests 1.75s (unchanged this step)
Manual setup and exact actions:
  Control smoke: Start-Process out/build/windows-debug/Debug/TracingApp.exe PID 40800 class TracingAppControlWindow hwnd 0x30F88. STATIC id 1004 included nav src=disabled, navBuffer=no, navRoi (none) navRoiSrc=none, packed-copy disclaimer. CloseMainWindow ExitCode=0 leftover=0. Status saved to out/manual/prompt20b-control-smoke.txt.
  Get-CimInstance Win32_Process CLIPStudioPaint.exe / CLIPStudio.exe: none. Did not Start Capture / Apply Nav ROI on a live Navigator panel / hide Navigator / pan/zoom/rotate. Did not inspect actual Navigator thumbnail/indicator pixels this step; detector remains the synthetic contrasting rectangle plus packed-BGRA Observe tests.
  Exact steps if a titled CLIP STUDIO PAINT document with a visible Navigator is available: select the PAINT row; Start Capture; Apply ROI for canvas as usual; enter client-relative Navigator x y w h covering the thumbnail (not chrome); Apply Nav ROI (optional Doc px fills thumbnail-to-document mapping); confirm status navBuffer=yes navRoiSrc=applied-mapped and that Start tracking / canvas roiFeed still work; Disable Nav and confirm tracking is unchanged and navBuffer=no; pan/zoom/rotate the canvas and hide Navigator — measured u/v/wu/hv should change or src=missing without pausing visual tracking. Do not treat relativeZoom as CSP %. Close TracingApp with CloseMainWindow.
Expected / actual:
  expected: second RoiReadback copies Navigator-kind ROI only when applied-mapped; missing/disabled source does not Detach TrackingSession or skip canvas roiFeed; packed BGRA Observe copies sequence/generations without inventing zoom%; empty packed view is Missing without document coords; Disable ignores later BGRA; control status navBuffer=no until a WGC navigator buffer exists; live CSP comparison if hardware allows
  actual: tests PASS as listed. Control smoke matches navBuffer=no navRoiSrc=none nav src=disabled. Live CSP Navigator NOT RUN. TrackingSession.cpp was not modified. Fusion remains Prompt 23.
Evidence paths (local, no private artwork committed; out/ is gitignored):
  out/build/windows-debug/Debug/TracingApp.exe
  out/build/windows-debug/Debug/TracingApp.NavigatorTests.exe
  out/manual/prompt20b-control-smoke.txt
Measured samples / p50 / p95 / max where applicable:
  synthetic packed-BGRA indicator center within 0.08 of known u (PackedBgraObserveCopiesMetadataWithoutZoomPercent); not MASTER 2 px / 0.5 deg claims
  live CSP Navigator residual / hide: NOT RUN
Known limitations / unavailable hardware:
  cmake/ctest not on PATH; invoked via VS 2026 bundled binaries
  Release configure/build/test NOT RUN this step
  no CLIPStudioPaint process this step; Navigator thumbnail appearance on CSP 5.0.0 theme was not inspected
  rectangle geometry never reports parity; relativeZoom is not CSP percent
  opencv imgcodecs not linked; TRACING_NAVIGATOR_FIXTURE cannot be decoded this step
  MASTER 2 px / 0.5 deg / 0.5% targets are not claimed for real CSP
  mixed-DPI and negative-origin monitors still absent
  Navigator observations are not fused into TrackingSession (Prompt 23)
Manual checklist remaining:
  1. Prompt 21 — Optional input prediction.
  2. Optional later: live CSP Navigator pan/zoom/rotation/themes/hidden on a titled document; live CSP flip; DXGI DDA and Windows 10 (1903+) OBS methods; per-pixel overlay transparency; CSP landmark eyeball.
Blocker or next prompt: 21 — Optional input prediction (do not auto-run)
```

## Final acceptance

Status: NOT RUN.

Do not change to accepted until mandatory gates have evidence. Optional observer unavailability may be an accepted documented limitation; failed OBS exclusion, input interception, unsafe confident tracking, or missing required verification cannot be waived silently.
