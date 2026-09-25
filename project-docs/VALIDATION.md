# TracingApp — validation record

Planning baseline: 2026-09-25. Prompt 01 Debug bootstrap compiled and tested on 2026-09-25. Prompt 02 pinned vcpkg/GoogleTest and verified Debug+Release on 2026-09-25. Prompt 03 added target discovery with automated selection tests; real CSP window selection remains NOT RUN. OBS recording and hardware acceptance testing remain NOT RUN.

Use PASS / FAIL / BLOCKED / NOT RUN. Each entry must include actual evidence. A successful API call or synthetic replay does not certify OBS behavior or real CSP tracking.

## Environment — fill when implementing

- App commit/build and renderer path: HEAD `bd02be3` (`feature/init`, Prompt 03 files uncommitted); Win32 control window with candidate list; Debug `out/build/windows-debug/Debug/TracingApp.exe` (829952 bytes, 2026-09-25 16:28:57); Release not rebuilt this step
- Windows edition/build and SDR/HDR state: registry `ProductName` Windows 10 Pro, `DisplayVersion` 25H2, build 26200.9457 (`EditionID` Professional). SDR/HDR NOT RECORDED
- Visual Studio/MSVC, Windows SDK, CMake, vcpkg baseline/triplet: Visual Studio Community 2026 18.10.2 at `C:\Program Files\Microsoft Visual Studio\18\Community`; MSVC 14.51.36231 / `cl` 19.51.36260.0 (`Hostx64/x64`); Windows SDK 10.0.26100.0; CMake/CTest 4.3.1-msvc1 (not on PATH) at `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`; generator Visual Studio 18 2026 x64; vcpkg VS 2026 bundled at `%VCPKG_ROOT%` = `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg` (not a git working copy); `vcpkg-bundle.json` `embeddedsha` / `vcpkg.json` `builtin-baseline` `1460b31b08c42cc2e9ac2c79f45ec8707e2675e2`; `vcpkg.exe version` `2026-07-27-98d7cb0cf1f4686a3e43aa5672b6230c1d56bce8`; triplet `x64-windows`
- OpenCV and GoogleTest resolved versions: OpenCV not introduced; GoogleTest `gtest:x64-windows@1.17.0#3` from git registry (`microsoft/vcpkg@dab84cf3bb50ef2ca3e0b0212c1d55e9e05a75bc`)
- C++/WinRT (verify-only, not linked): SDK `10.0.26100.0` headers present at `C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt\winrt\base.h` and `windows.graphics.capture.h`; `cppwinrt.exe` v2.0.250303.5. Windows App SDK not adopted.
- GPU, driver, monitor sizes/refresh/DPI/origins: NVIDIA GeForce RTX 3060 driver 32.0.16.1656 at 2560x1440 (active); AMD Radeon(TM) Graphics driver 32.0.21042.62 (no current resolution reported). Refresh/DPI/origins NOT RECORDED
- CSP version/locale/theme/workspace and drawing device: NOT RECORDED
- OBS version, Display Capture method/settings, recording resolution/FPS: NOT RECORDED

## Milestone status

- M0 reproducible shell/build/tests: PASS (Prompts 01–02: local Git, Win32 control window, vcpkg baseline, first-party `/W4` `/permissive-`, GoogleTest, Debug+Release `--fresh` configure/build/test. DPI manifest remains Prompt 04)
- M1 CSP discovery/geometry/lifecycle: NOT RUN (Prompt 03 automated filter/selection tests PASS; real CSP enumerate/select, geometry, and lifecycle remain later / unavailable this step)
- M2 transparency/input/affinity: NOT RUN
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

## Final acceptance

Status: NOT RUN.

Do not change to accepted until mandatory gates have evidence. Optional observer unavailability may be an accepted documented limitation; failed OBS exclusion, input interception, unsafe confident tracking, or missing required verification cannot be waived silently.
