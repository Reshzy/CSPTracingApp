# TracingApp — validation record

Planning baseline: 2026-09-25. Prompt 01 Debug bootstrap compiled and tested on 2026-09-25. Prompt 02 pinned vcpkg/GoogleTest and verified Debug+Release on 2026-09-25. Prompt 03 added target discovery with automated selection tests; real CSP window selection remains incomplete. Prompt 04 embedded PerMonitorV2 DPI awareness and target geometry/lifecycle reporting; real CSP mixed-DPI/lifecycle remains NOT RUN. Prompt 05 created a BGRA D3D11 device with RAII immediate-context ownership and GPU-free lifecycle tests. Prompt 06 added a DirectComposition overlay HWND, WDA_EXCLUDEFROMCAPTURE before show, emergency hide, and GPU-free visibility tests. Prompt 07 added tracing/alignment modes and a separate-process InputProbe; the DirectComposition HWND/style combination failed cross-process mouse/wheel pass-through. Prompt 07-fallback replaced that path with an isolated WS_EX_LAYERED UpdateLayeredWindow presenter; InputProbe click/wheel pass-through now PASSes on both monitors, and tracing hits the titled CSP HWND without overlay activation. Prompt 08 added a C++/WinRT WGC session with a free-threaded frame pool, owned-frame handoff, and control-window counters/metadata. Prompt 09 added an opt-in ordinary WDA_NONE capture preview, frame-pool Recreate on content-size change, measured capture-to-client mapping, and overlay hide on capture loss. Prompt 10 added Unicode IFileOpenDialog plus WIC PNG/JPEG decode into an owned 32bppPBGRA buffer with 16,384/axis and 256-MiB limits. Prompt 11 uploads that buffer once to an immutable D3D11 texture, compiles shaders/Image.hlsl with discovered SDK fxc, and draws a premultiplied textured quad onto the existing layered overlay HWND with opacity/fit/reset. Prompt 12 adds a separate ordinary `WDA_NONE` ReferenceWindow with its own DXGI swap chain that reuses that immutable texture; overlay exclusion stays `WDA_EXCLUDEFROMCAPTURE`. Pen/pressure remains NOT RUN. OBS recording remains Prompt 13.

Use PASS / FAIL / BLOCKED / NOT RUN. Each entry must include actual evidence. A successful API call or synthetic replay does not certify OBS behavior or real CSP tracking.

## Environment — fill when implementing

- App commit/build and renderer path: HEAD `38ff8b1` (`feature/init`, Prompt 11–12 files uncommitted); Win32 control window plus isolated WS_EX_LAYERED overlay HWND (WDA_EXCLUDEFROMCAPTURE) and a separate ordinary WS_OVERLAPPEDWINDOW ReferenceWindow (WDA_NONE, DXGI swap chain); WIC decode still owns the CPU 32bppPBGRA buffer; ImageRenderer uploads it once to an IMMUTABLE DXGI_FORMAT_B8G8R8A8_UNORM texture, presents a premul textured quad onto the overlay HWND via UpdateLayeredWindow, and DrawQuad reuses the same SRV on the reference RTV (opaque dark-gray clear, independent fit, opacity 1.0); OverlaySurface still draws the test marker first; image present overwrites ULW when a texture exists; no DirectComposition; shaders compiled with discovered `fxc.exe` `10.0.26100.8249` at `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe`; WGC session copies owned D3D textures, Recreates the free-threaded pool on content-size change, and can show an ordinary WDA_NONE debug preview (off by default; import/show-reference does not enable it); Debug `out/build/windows-debug/Debug/TracingApp.exe` (2241536 bytes, 2026-09-25 20:30:55) and `TracingApp.InputProbe.exe` (70144 bytes, 2026-09-25 18:02:00); Release not rebuilt this step
- Windows edition/build and SDR/HDR state: registry `ProductName` Windows 10 Pro, `DisplayVersion` 25H2, build 26200.9457 (`EditionID` Professional). SDR/HDR NOT RECORDED
- Visual Studio/MSVC, Windows SDK, CMake, vcpkg baseline/triplet: Visual Studio Community 2026 18.10.2 at `C:\Program Files\Microsoft Visual Studio\18\Community`; MSVC 14.51.36231 / `cl` 19.51.36260.0 (`Hostx64/x64`); Windows SDK 10.0.26100.0; CMake/CTest 4.3.1-msvc1 (not on PATH) at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`; generator Visual Studio 18 2026 x64; vcpkg VS 2026 bundled at `%VCPKG_ROOT%` = `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg` (not a git working copy); `vcpkg-bundle.json` `embeddedsha` / `vcpkg.json` `builtin-baseline` `1460b31b08c42cc2e9ac2c79f45ec8707e2675e2`; `vcpkg.exe version` `2026-07-27-98d7cb0cf1f4686a3e43aa5672b6230c1d56bce8`; triplet `x64-windows`
- OpenCV and GoogleTest resolved versions: OpenCV not introduced; GoogleTest `gtest:x64-windows@1.17.0#3` from git registry (`microsoft/vcpkg@dab84cf3bb50ef2ca3e0b0212c1d55e9e05a75bc`)
- C++/WinRT: SDK `10.0.26100.0` headers; TracingApp links `WindowsApp.lib` and uses `IGraphicsCaptureItemInterop::CreateForWindow` plus `Direct3D11CaptureFramePool::CreateFreeThreaded`. `cppwinrt.exe` v2.0.250303.5. Windows App SDK not adopted.
- GPU, driver, monitor sizes/refresh/DPI/origins: NVIDIA GeForce RTX 3060 driver 32.0.16.1656; AMD Radeon(TM) Graphics driver 32.0.21042.62. Prompt 05 default DXGI adapter was `NVIDIA GeForce RTX 3060` vendor=0x10DE device=0x24C7, D3D_FEATURE_LEVEL_11_1. Screens: `\\.\DISPLAY2` primary Bounds={X=0,Y=0,Width=1920,Height=1080} effective DPI 96x96; `\\.\DISPLAY1` Bounds={X=1920,Y=0,Width=2560,Height=1440} effective DPI 96x96. Refresh NOT RECORDED. Negative-origin monitor not present (`MonitorFromPoint(-100,100)` resolved to the primary). Mixed-DPI not present (both 96).
- CSP version/locale/theme/workspace and drawing device: CLIPStudioPaint.exe 5.0.0 (`FileVersion` 5.0.0.0) at `C:\Program Files\CELSYS\CLIP STUDIO 1.5\CLIP STUDIO PAINT\CLIPStudioPaint.exe` pid 36888 during Prompt 06; CLIPStudio.exe launcher pid 20700. Locale/theme/workspace/pen NOT RECORDED
- OBS version, Display Capture method/settings, recording resolution/FPS: NOT RECORDED

## Milestone status

- M0 reproducible shell/build/tests: PASS (Prompts 01–02 plus Prompt 04 DPI manifest: local Git, Win32 control window, PerMonitorV2 RT_MANIFEST, vcpkg baseline, first-party `/W4` `/permissive-`, GoogleTest, Debug configure/build/test)
- M1 CSP discovery/geometry/lifecycle: NOT RUN (Prompt 03 automated tests PASS; Prompt 06 enumerated live CLIPStudioPaint.exe and selected one PAINT row; mixed-DPI move, minimize/restore/close, and titled main-window vs panel disambiguation remain incomplete)
- M2 transparency/input/affinity: PASS for layered mouse/wheel pass-through (Prompt 07-fallback). Prompt 05 device PASS; Prompt 06 affinity/emergency hide/visibility policy PASS; Prompt 07 DComp path FAIL (superseded). Layered `WS_EX_LAYERED` + tracing `WS_EX_TRANSPARENT` delivers InputProbe click/wheel on both monitors without overlay activation; titled CSP HWND hit-test also skips the overlay. Pen/pressure NOT RUN. OBS Display Capture remains Prompt 13.
- M3 actual CSP capture/resize/no-feedback: PASS for titled CLIP STUDIO PAINT WGC preview + pool Recreate + marker-absent-from-capture-texture (Prompt 09). Prompt 08 counters/content-size remain PASS. Item-closed (CSP close while capturing) NOT RUN. OBS Display Capture remains Prompt 13.
- M4 imported image/ordinary reference/OBS playback: PARTIAL — WIC PNG/JPEG Unicode import PASS (Prompt 10); imported-image overlay renderer PASS (Prompt 11); ordinary WDA_NONE ReferenceWindow reusing the GPU texture PASS (Prompt 12 Debug present/affinity/independent fit/opacity/close-vs-capture/focus policy). OBS Display Capture remains Prompt 13. Mandatory OBS gate still NOT RUN before tracking.
- M5 manual transforms/calibration: NOT RUN
- M6 visual/parity tracking and confidence safety: NOT RUN
- M7 hybrid observers/fusion: NOT RUN
- M8 integration/performance/final recording regression: NOT RUN

## Manual checklist

- [ ] Actual CSP painting window selected; ambiguous instances require selection.
- [ ] Physical coordinates correct at 100%, 125%, 150%, 200% scale where available.
- [ ] Negative-origin and mixed-DPI monitor movement tested or explicitly not available.
- [ ] Overlay transparent outside image and clipped to selected canvas.
- [ ] Mouse, wheel and pen/pressure reach CSP; overlay does not take focus.
- [ ] Interactive alignment toggles back to tracing without stuck input.
- [ ] Emergency hide works; minimize/target close/unrelated focus hide appropriately.
- [ ] Modal dialogs/floating palettes do not have misleading tracing content above them.
- [ ] Captured CSP pixels update; resize/stale/closed capture handled.
- [ ] Capture/client offsets calibrated; overlay absent from actual CSP capture.
- [x] PNG/JPEG/Unicode path/corrupt/oversized images handled.
- [x] Ordinary reference HWND remains WDA_NONE and independent of tracking.
- [ ] OBS Display Capture positive control records both nonsensitive markers.
- [ ] Exclusion test playback omits private marker with intact underlying CSP.
- [ ] Ordinary reference visible in playback throughout its visible intervals.
- [ ] No private-frame flash/black rectangle during show/hide/resize/recreation.
- [ ] Each intended OBS method and monitor configuration tested separately.
- [ ] Manual pan/zoom/rotation/flips obey known asymmetric landmarks.
- [ ] Visual tracking tested with textured drawing and new strokes.
- [ ] Blank/symmetric scenes and ambiguous flips degrade/hide safely.
- [ ] Navigator disappearance and optional observer disablement handled.
- [ ] Stale/wrong-generation observations cannot move overlay.
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

## Final acceptance

Status: NOT RUN.

Do not change to accepted until mandatory gates have evidence. Optional observer unavailability may be an accepted documented limitation; failed OBS exclusion, input interception, unsafe confident tracking, or missing required verification cannot be waived silently.
