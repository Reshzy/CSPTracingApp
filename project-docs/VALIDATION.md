# TracingApp — validation record

Planning baseline: 2026-09-25. Prompt 01 Debug bootstrap compiled and tested on 2026-09-25. Prompt 02 pinned vcpkg/GoogleTest and verified Debug+Release on 2026-09-25. Prompt 03 added target discovery with automated selection tests; real CSP window selection remains incomplete. Prompt 04 embedded PerMonitorV2 DPI awareness and target geometry/lifecycle reporting; real CSP mixed-DPI/lifecycle remains NOT RUN. Prompt 05 created a BGRA D3D11 device with RAII immediate-context ownership and GPU-free lifecycle tests. Prompt 06 added a DirectComposition overlay HWND, WDA_EXCLUDEFROMCAPTURE before show, emergency hide, and GPU-free visibility tests. OBS recording and input pass-through remain later.

Use PASS / FAIL / BLOCKED / NOT RUN. Each entry must include actual evidence. A successful API call or synthetic replay does not certify OBS behavior or real CSP tracking.

## Environment — fill when implementing

- App commit/build and renderer path: HEAD `4b61844` (`feature/init`, Prompt 06 files uncommitted); Win32 control window plus hidden DirectComposition overlay HWND (test-pattern marker, no imported-image renderer, no WGC); Debug `out/build/windows-debug/Debug/TracingApp.exe` (935424 bytes, 2026-09-25 17:20:00); Release not rebuilt this step
- Windows edition/build and SDR/HDR state: registry `ProductName` Windows 10 Pro, `DisplayVersion` 25H2, build 26200.9457 (`EditionID` Professional). SDR/HDR NOT RECORDED
- Visual Studio/MSVC, Windows SDK, CMake, vcpkg baseline/triplet: Visual Studio Community 2026 18.10.2 at `C:\Program Files\Microsoft Visual Studio\18\Community`; MSVC 14.51.36231 / `cl` 19.51.36260.0 (`Hostx64/x64`); Windows SDK 10.0.26100.0; CMake/CTest 4.3.1-msvc1 (not on PATH) at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`; generator Visual Studio 18 2026 x64; vcpkg VS 2026 bundled at `%VCPKG_ROOT%` = `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg` (not a git working copy); `vcpkg-bundle.json` `embeddedsha` / `vcpkg.json` `builtin-baseline` `1460b31b08c42cc2e9ac2c79f45ec8707e2675e2`; `vcpkg.exe version` `2026-07-27-98d7cb0cf1f4686a3e43aa5672b6230c1d56bce8`; triplet `x64-windows`
- OpenCV and GoogleTest resolved versions: OpenCV not introduced; GoogleTest `gtest:x64-windows@1.17.0#3` from git registry (`microsoft/vcpkg@dab84cf3bb50ef2ca3e0b0212c1d55e9e05a75bc`)
- C++/WinRT (verify-only, not linked): SDK `10.0.26100.0` headers present at `C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt\winrt\base.h` and `windows.graphics.capture.h`; `cppwinrt.exe` v2.0.250303.5. Windows App SDK not adopted.
- GPU, driver, monitor sizes/refresh/DPI/origins: NVIDIA GeForce RTX 3060 driver 32.0.16.1656; AMD Radeon(TM) Graphics driver 32.0.21042.62. Prompt 05 default DXGI adapter was `NVIDIA GeForce RTX 3060` vendor=0x10DE device=0x24C7, D3D_FEATURE_LEVEL_11_1. Screens: `\\.\DISPLAY2` primary Bounds={X=0,Y=0,Width=1920,Height=1080} effective DPI 96x96; `\\.\DISPLAY1` Bounds={X=1920,Y=0,Width=2560,Height=1440} effective DPI 96x96. Refresh NOT RECORDED. Negative-origin monitor not present (`MonitorFromPoint(-100,100)` resolved to the primary). Mixed-DPI not present (both 96).
- CSP version/locale/theme/workspace and drawing device: CLIPStudioPaint.exe 5.0.0 (`FileVersion` 5.0.0.0) at `C:\Program Files\CELSYS\CLIP STUDIO 1.5\CLIP STUDIO PAINT\CLIPStudioPaint.exe` pid 36888 during Prompt 06; CLIPStudio.exe launcher pid 20700. Locale/theme/workspace/pen NOT RECORDED
- OBS version, Display Capture method/settings, recording resolution/FPS: NOT RECORDED

## Milestone status

- M0 reproducible shell/build/tests: PASS (Prompts 01–02 plus Prompt 04 DPI manifest: local Git, Win32 control window, PerMonitorV2 RT_MANIFEST, vcpkg baseline, first-party `/W4` `/permissive-`, GoogleTest, Debug configure/build/test)
- M1 CSP discovery/geometry/lifecycle: NOT RUN (Prompt 03 automated tests PASS; Prompt 06 enumerated live CLIPStudioPaint.exe and selected one PAINT row; mixed-DPI move, minimize/restore/close, and titled main-window vs panel disambiguation remain incomplete)
- M2 transparency/input/affinity: NOT RUN (Prompt 05 device PASS; Prompt 06 overlay HWND, test-pattern, WDA_EXCLUDEFROMCAPTURE set+readback, emergency hide, and conservative visibility policy PASS; mouse/wheel/pen pass-through remains Prompt 07; OBS Display Capture remains Prompt 13)
- M3 actual CSP capture/resize/no-feedback: NOT RUN
- M4 imported image/ordinary reference/OBS playback: NOT RUN — mandatory before tracking
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
- [ ] PNG/JPEG/Unicode path/corrupt/oversized images handled.
- [ ] Ordinary reference HWND remains WDA_NONE and independent of tracking.
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

## Final acceptance

Status: NOT RUN.

Do not change to accepted until mandatory gates have evidence. Optional observer unavailability may be an accepted documented limitation; failed OBS exclusion, input interception, unsafe confident tracking, or missing required verification cannot be waived silently.
