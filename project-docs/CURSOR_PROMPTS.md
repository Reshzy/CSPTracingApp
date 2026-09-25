# TracingApp — incremental Cursor prompts

Run these prompts in order in a new implementation repository containing `project-docs/`. Paste one fenced prompt at a time. Each prompt explicitly incorporates the execution contract below; Cursor must read this file, not rely on chat memory. Do not batch the entire sequence into one request.

## Shared execution contract

Every numbered prompt means:

1. Read applicable AGENTS.md and `project-docs/MASTER.md`, this contract, and the latest `project-docs/VALIDATION.md`. Preserve existing work and read-only synced sources. Inspect current code before editing. Implement only this step.
2. Change at most **five implementation/configuration/test files**, plus one update to VALIDATION.md. The suggested paths are a scope guide: use existing equivalent files instead of duplicate modules. New files count. If the task cannot fit, split it into a smaller buildable substep and finish that substep first. List the remaining substep; do not quietly exceed the limit.
3. Use real APIs and real errors. No fabricated tool versions, hashes, capture frames, capability claims, performance figures or test results. Any temporary test pattern must be labelled as such. Do not introduce WinUI or an unrelated framework.
4. After every code change run `cmake --preset windows-debug`, `cmake --build --preset windows-debug`, and `ctest --preset windows-debug --output-on-failure`. At least one meaningful test must be discovered. Fix failures caused by the step within scope, then rerun affected checks. If blocked by missing tools/dependencies, report the exact blocker and stop dependent work. Bootstrap prompt 01 must establish these commands before declaring completion.
5. Run the step's manual checks if the real applications/devices are accessible. Otherwise mark them NOT RUN and provide precise steps. Never advance past a required hardware/privacy gate on unverified assumptions. Automated tests do not certify OBS or pen behavior.
6. Update VALIDATION.md with actual commands/outcomes and manual evidence or blockers. Finish with changed files, behavior, verification, remaining limits, and next prompt/substep. Do not auto-run that next prompt, push a repository, or publish anything.

## 01 — Initialize a buildable repository

```text
Implement step 01 under the Shared execution contract in project-docs/CURSOR_PROMPTS.md. Read MASTER.md first. Inspect this folder and its Git status. Initialize local Git only if no repository exists; do not create a remote or touch synced sources. Create at most five files: .gitignore, CMakeLists.txt, CMakePresets.json, src/app/main.cpp, and tests/unit/bootstrap_tests.cpp. Build the smallest C++23 Unicode Win32 application with a normal control window and clean exit. Add a dependency-free CTest executable that verifies a small genuine application policy such as rejecting zero viewport dimensions; avoid a test that only returns success. Use the installed Visual Studio 2026 toolchain and CMake Visual Studio 18 2026 x64 generator, checking availability first. Define windows-debug/windows-release configure, build and test presets with explicit configurations. Do not add capture, OpenCV or rendering yet. Run the Debug configure/build/test commands; record exact installed tool versions and results. Launch and close the shell if possible. Do not mark bootstrap complete without a real executable and discovered test.
```

## 02 — Pin dependencies and compiler policy

```text
Implement step 02 under the Shared execution contract. Scope: vcpkg.json, CMakePresets.json, CMakeLists.txt, tests/unit/bootstrap_tests.cpp, and .clang-format. Inspect the installed vcpkg checkout and record a real baseline commit. Add GoogleTest using manifest mode and x64-windows; make toolchain resolution use VCPKG_ROOT without personal absolute paths. Convert the existing meaningful test to GoogleTest and preserve its assertion. Request C++23, Unicode, /W4 and /permissive- on first-party targets, consistent runtime linking, and clean dependency warning boundaries. Do not add OpenCV until the tracking stage. Verify SDK C++/WinRT header availability without adopting Windows App SDK. Run Debug and Release configure/build/test; report unavailable dependencies accurately instead of inventing versions. Keep an offline/network failure distinct from a source compilation failure.
```

## 03 — Target discovery

```text
Implement step 03 under the Shared execution contract. Scope: src/platform/TargetDiscovery.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/TargetSelectionTests.cpp. Enumerate top-level candidates with EnumWindows, collect HWND/PID/process identity and visibility, and identify likely CSP candidates without depending only on a localized title or guessed class. Display candidates and allow explicit selection. Represent target identity with a session generation and enough process identity to detect handle reuse. Keep candidate filtering separately testable; test ambiguous candidates and rejected dead targets. Show actionable access failures. Do not inject into CSP or request admin rights as a default. Build/test, then select a real CSP painting window and verify that a launcher or second instance cannot silently replace it.
```

## 04 — Physical geometry and lifecycle

```text
Implement step 04 under the Shared execution contract. Scope: resources/app.manifest, src/platform/TargetGeometry.h/.cpp, src/app/main.cpp, CMakeLists.txt. Embed per-monitor-V2 DPI awareness. Report physical client origin/size, DPI and target generation separately from outer-window bounds. Use WinEvent notifications or modest bounded polling for move/resize/minimize/destroy/foreground changes; callbacks only queue work. Validate HWND and process identity before use. Invalidate geometry on target replacement. Avoid presenting geometry as canvas bounds. Build/test and manually move CSP between differently scaled monitors, including a negative-origin monitor if available; minimize, restore and close it. Record expected versus observed geometry and any untested configuration.
```

## 05 — Shared D3D11 device

```text
Implement step 05 under the Shared execution contract. Scope: src/graphics/DeviceResources.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/GraphicsPolicyTests.cpp. Introduce a BGRA-capable D3D11 device with explicit immediate-context ownership and RAII. Enable the debug layer when installed and distinguish its absence from device creation failure. Record adapter identity, feature level and removed-device error. Add an injectable small lifecycle policy so device invalidation/retry transitions can be tested without requiring a GPU. Do not add a renderer or capture session yet. Run Debug build/tests and a real device-create/shutdown smoke test; report live-object warnings if diagnostics are available.
```

## 06 — Transparent overlay surface and affinity

```text
Implement step 06 under the Shared execution contract. Scope: src/graphics/OverlaySurface.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/OverlayPolicyTests.cpp. Create our own top-level nonactivating tool popup and an alpha-capable DXGI composition swap chain attached through DirectComposition. Render a transparent background with an asymmetric colored test marker. Apply WDA_EXCLUDEFROMCAPTURE before showing the marker, check errors/readback, and keep it hidden on failure. Provide emergency hide and a conservative target-visible/foreground policy. Keep display-affinity success separate from OBS verification status. Test visibility decisions as pure policy. Build/test; manually verify transparency, position and focus behavior. This is a test-pattern surface, not an imported-image renderer.
```

## 07 — Prove drawing pass-through

```text
Implement step 07 under the Shared execution contract. Scope: src/graphics/OverlaySurface.h/.cpp, src/app/main.cpp, tests/harness/InputProbe.cpp, CMakeLists.txt. Prove that mouse, wheel and pen input reaches a DIFFERENT process beneath the overlay without activating the overlay. Do not assume WS_EX_TRANSPARENT or HTTRANSPARENT alone guarantees it. Add a tiny separate-process input probe with event counts, and test the actual HWND/compositor style combination. Implement explicit interactive alignment mode versus noninteractive tracing mode. Preserve emergency hide. If composition cannot satisfy pass-through, stop with the failing evidence and propose a bounded layered-window fallback substep; do not rewrite the renderer wholesale. Build/test and verify actual CSP pen strokes/pressure if hardware is available. Record omissions. This gate must pass before treating the overlay as usable.
```

## 08 — WGC session and owned frames

```text
Implement step 08 under the Shared execution contract. Scope: src/capture/CaptureSession.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/CaptureStateTests.cpp. Capture the selected CSP HWND using IGraphicsCaptureItemInterop::CreateForWindow and C++/WinRT, with runtime support checks and a free-threaded frame pool. Wrap the D3D device correctly. Define owned frame handoff, sequence/capture timestamps/content size and target generation; callbacks must not race the immediate context or perform CV. Bound pending work and release frames deterministically. Implement stop and item-closed handling, with generation/state tests. Initially expose frame counters and metadata in the control window. Build/test and confirm counters/content sizes change on a real CSP window. Do not call a generated test texture a captured frame.
```

## 09 — Capture preview, resize and stale-frame handling

```text
Implement step 09 under the Shared execution contract. Scope: src/capture/CaptureSession.cpp, src/graphics/CapturePreview.h/.cpp, src/app/main.cpp, CMakeLists.txt. Display actual WGC frames in an explicitly enabled ordinary debug preview. Serialize GPU access, copy to owned resources before source lifetime ends, and handle content-size changes/frame-pool recreation safely. Distinguish capture texture origin from client origin; expose measured mapping for calibration instead of hardcoding offsets. Label stale/zero frames and hide the tracing surface on capture loss. Disable preview by default for the recording workflow. Build/test, then draw in CSP, resize/minimize/restore/close it and confirm preview behavior. Show the private marker while capturing CSP and verify the marker does not feed back into CSP frames. Record all results.
```

## 10 — WIC image import

```text
Implement step 10 under the Shared execution contract. Scope: src/image/ImageLoader.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/ImageLoaderTests.cpp. Add Unicode file selection and WIC PNG/JPEG decoding. Define orientation normalization and BGRA/premultiplied-alpha ownership explicitly. Check dimensions, multiplication overflow, 16,384-per-axis and 256-MiB decoded limits before allocating. Preserve the previous valid image if loading fails. Add tests using generated tiny fixtures for dimensions/alpha and corrupt/oversized inputs; do not commit private artwork. Keep decoding independent of the overlay. Run build/tests and manually import valid PNG/JPEG plus a corrupt file, including a Unicode path.
```

## 11 — Image renderer

```text
Implement step 11 under the Shared execution contract. Scope: src/graphics/ImageRenderer.h/.cpp, shaders/Image.hlsl, src/app/main.cpp, CMakeLists.txt. Upload the decoded image once and draw a textured quad on the existing overlay surface. Compile/package the shader reproducibly through the build using discovered tools; no developer-specific paths. Define and verify premultiplied alpha and transparent clear behavior. Add opacity, fit and reset controls through the existing window. Initially use explicit image placement without automatic tracking. Reuse the established D3D ownership policy. Build/test and inspect transparent PNG edges, JPEG, opacity 0/0.5/1 and resizing against a checkerboard/asymmetric asset. Keep the reference texture unchanged while adjusting placement.
```

## 12 — Ordinary reference window

```text
Implement step 12 under the Shared execution contract. Scope: src/app/ReferenceWindow.h/.cpp, src/app/main.cpp, src/graphics/ImageRenderer.cpp, CMakeLists.txt. Add a separate ordinary movable reference window that reuses the reference image texture and appropriate independent presentation resources. Its top-level HWND must have WDA_NONE; exclusion belongs only to the private tracing HWND. Reference visibility and fit should be independent of tracking. Do not show capture/debug mirrors during the normal recording workflow. Build/test and verify both windows display the correct image, closing the reference does not stop capture, and focus changes follow the overlay's documented visibility policy.
```

## 13 — OBS Display Capture exclusion gate

```text
Execute step 13 under the Shared execution contract and MASTER.md section 8. Change no implementation files unless a concrete defect is found; keep any fix within five files and rebuild/retest it. Use a LOCAL OBS recording with Display Capture, not Window Capture. Record OS/GPU/driver/DPI/HDR/OBS version and capture-method label. With nonsensitive test markers, establish the WDA_NONE positive control, then restore WDA_EXCLUDEFROMCAPTURE. Confirm both windows are physically visible but saved playback shows only CSP and the ordinary reference, with no private marker or black replacement. Exercise opacity, move/resize, show/hide and HWND recreation. Inspect saved playback, not only preview. Repeat for each available method intended for use and record results separately. If OBS cannot be operated, mark NOT RUN and give the exact manual checklist; STOP this gate. If exclusion fails, mark BLOCKED and diagnose before any tracking work. Do not claim the Windows API return value proves OBS compatibility.
```

## 14 — Pure transform engine

```text
Implement step 14 under the Shared execution contract only after the OBS gate passes. Scope: src/core/Transform2D.h/.cpp, tests/unit/TransformTests.cpp, CMakeLists.txt, src/app/main.cpp. Implement double-precision homogeneous 2D transforms using MASTER.md's column-vector, X-right/Y-down convention and named spaces R/D/S/O/C. Include composition, inverse, translation, uniform scale, rotation and reflection, with finite/singular checks. Keep M_RD separate from M_DS and compute overlay-local output explicitly. Test independent known point results, noncommuting order, off-center pivots, both flip axes, equivalent parity representations, negative monitor origins and inverse round trips. Do not claim canonical storage removes measurement drift. Build/test; expose a small numerical diagnostic without connecting visual tracking yet.
```

## 15 — Manual calibration and viewport clipping

```text
Implement step 15 under the Shared execution contract. Scope: src/core/Calibration.h/.cpp, src/app/main.cpp, src/graphics/ImageRenderer.cpp, tests/unit/CalibrationTests.cpp. Let the user explicitly select canvas ROI, enter document dimensions if known, and align reference position/scale/rotation/flip. Distinguish calibrated local document units from verified document pixels/zoom percent. Implement validated capture-to-client/screen mapping and overlay-local clipping. Window movement must change screen origin without changing M_RD. Invalidate calibration on incompatible geometry/layout/document changes. Test coordinate conversions and invalid ROIs. Build/test and manually compare asymmetric landmarks through pan/zoom/rotation/flip controls and mixed-DPI window movement. Automatic tracking remains disabled.
```

## 16 — Bounded ROI readback

```text
Implement step 16 under the Shared execution contract. Scope: src/capture/RoiReadback.h/.cpp, src/capture/CaptureSession.cpp, CMakeLists.txt, tests/unit/RoiTests.cpp. Add checked canvas/Navigator ROI extraction, optional downsampling and a small staging-resource ring. Respect D3D row pitch and never map before the copy is ready or block the UI waiting for CV. Keep source capture timestamp, sequence, geometry and target generations with CPU buffers. Drop obsolete work rather than accumulating a queue. Test clipping/size arithmetic/stride conversion using synthetic buffers. Build/test; measure actual copy/map wait and bounded pending count on CSP, including resize. Do not claim the CPU OpenCV path is zero-copy.
```

## 17 — OpenCV visual estimator

```text
Implement step 17 under the Shared execution contract. Scope: vcpkg.json, CMakeLists.txt, src/tracking/VisualTracker.h/.cpp, tests/unit/VisualTrackerTests.cpp. Add only the required OpenCV modules using the existing real pinned baseline; verify the resolved version. Track canvas pixels against prior canvas/keyframe pixels, never against the imported tracing reference. Implement robust translation/uniform-scale/rotation estimation with outlier rejection, spatial coverage, residual and confidence metrics. Reject blank/degenerate scenes and implausible/sheared fits. Add deterministic synthetic textured-image tests with known transforms and changing-stroke/outlier regions. Keep this estimator independent of rendering and input prediction. Build/test and report actual errors; do not introduce reflection support by pretending a positive-scale partial affine fit handles it.
```

## 18 — Tracking state and baseline integration

```text
Implement step 18 under the Shared execution contract. Scope: src/tracking/TrackingSession.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/TrackingSessionTests.cpp. Connect bounded ROI buffers to the estimator and publish immutable absolute transform snapshots. Use calibrated keyframe anchors, matching generations and capture timestamps. Implement Unattached/Calibrating/Tracking/Degraded/Lost/Paused/Unavailable with MASTER.md's provisional age limits and clear invalidation. Hide on Lost instead of leaving misleading alignment. Never join while holding callback locks. Test stale/out-of-order frames, wrong generations, contradictory measurements and consistent reacquisition. Build/test and try real textured CSP pan/zoom/rotation, then a blank canvas and minimize/restore. Report observed registration quality and confidence behavior, not just apparent smoothness.
```

## 19 — Reflection hypotheses and relocalization

```text
Implement step 19 under the Shared execution contract. Scope: src/tracking/VisualTracker.h/.cpp, src/tracking/TrackingSession.cpp, tests/unit/VisualTrackerTests.cpp, tests/unit/TrackingSessionTests.cpp. Add explicit mirrored-image hypotheses with correct coordinate remapping before the orientation-preserving fit. Use asymmetric evidence and hysteresis to change parity; handle equivalent two-flip/rotation representations consistently. Add keyframe relocalization to limit drift without promoting unreliable anchors. Test each flip axis, combined flips/rotation, symmetric ambiguity, prolonged outliers and return to a known keyframe. Build/test, then exercise CSP flips with an asymmetric drawing. If parity is ambiguous, pause/request resync instead of reporting success. Record drift and reacquisition separately.
```

## 20 — Navigator observer

```text
Implement step 20 under the Shared execution contract. Scope: src/tracking/NavigatorObserver.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/NavigatorTests.cpp. Add user-calibrated Navigator ROI observation. Inspect what the installed CSP actually renders before selecting image features/viewport-indicator geometry. Return only measurable components with confidence and timestamp; do not invent exact zoom/rotation or document coordinates from an uncalibrated thumbnail. No OCR dependency in this step. Tests should use synthetic representative indicators with missing/occluded/ambiguous cases; identify any real-CSP fixture as local opt-in evidence. Build/test; compare measured movement with actual CSP pan/zoom/rotation, themes and hidden Navigator. Missing Navigator must disable this source cleanly without breaking existing manual/visual modes.
```

## 21 — Optional input prediction

```text
Implement step 21 under the Shared execution contract. Scope: src/platform/InputObserver.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/InputPredictionTests.cpp. Add optional Raw Input observation gated to the selected foreground CSP session. Do not suppress, inject, globally record, or assume every physical event reached CSP. Use a user-declared gesture mapping; keep prediction disabled by default until calibrated. Predict only a short bounded interval, expiring within 100 ms without visual confirmation. Test wrong foreground, unknown gestures, expiry, generation change and contradictory visual correction. Build/test and compare enabled/disabled behavior. Preserve pen drawing and existing mouse messages; do not implement low-level global hooks unless a measured requirement justifies a later dedicated step.
```

## 22 — Optional accessibility capability probe

```text
Implement step 22 under the Shared execution contract. Scope: src/platform/AccessibilityObserver.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/ObserverCapabilityTests.cpp. Add an optional UI Automation inspection/capability adapter for the selected CSP target. Determine whether relevant Navigator/zoom/rotation values actually exist in its accessible tree. No hardcoded claim of CSP support, no UI actions, and no blocking UI-thread traversal. Bound work, handle target close and provider errors, and avoid retaining stale elements. If values are unavailable, report unsupported and leave the adapter disabled; this is a valid outcome. Test unavailable/stale adapter states. Build/test and document only observed accessible properties with CSP version/locale. Do not depend on this adapter for baseline tracking.
```

## 23 — Timestamped hybrid fusion

```text
Implement step 23 under the Shared execution contract. Scope: src/tracking/TransformFusion.h/.cpp, src/tracking/TrackingSession.cpp, CMakeLists.txt, tests/unit/FusionTests.cpp. Fuse visual, Navigator and optional observer predictions in common calibrated coordinates and timestamps. Reject stale/wrong-generation observations, gate conflicting measurements and prefer confirmed anchors over input prediction. Fuse translation/log-scale/unwrapped angle only within a consistent parity hypothesis; never linearly average reflection matrices. Publish quality and source contributions. Test observer dropout, delayed frames, wraparound, contradictory parity, input correction and relocalization. Build/test and compare the same replay with visual-only versus hybrid mode. Accept hybrid only if evidence improves latency/recovery without increasing confident wrong estimates; retain visual/manual fallback.
```

## 24 — Diagnostics and privacy-aware evidence

```text
Implement step 24 under the Shared execution contract. Scope: src/diagnostics/Metrics.h/.cpp, src/app/main.cpp, CMakeLists.txt, tests/unit/MetricsTests.cpp. Add bounded metrics for capture age, dropped frames, staging wait, CV/fusion durations, confidence/inliers/residual, parity, state, DPI/ROI, device errors and affinity status. Show OBS verification as a separate user-recorded status, not an automatically certified green light. Summarize p50/p95/max over a bounded window. Default logs must omit image pixels/full private paths/raw input. Make image replay recording explicit opt-in with local output and size limits. Test aggregation/limits. Build/test and verify a long run does not grow log buffers indefinitely or reveal the private overlay through debug previews.
```

## 25 — Deterministic replay regression suite

```text
Implement step 25 under the Shared execution contract. Scope: tests/replay/SequenceGenerator.h/.cpp, tests/replay/TrackingReplayTests.cpp, tests/replay/README.md, CMakeLists.txt. Generate seeded textured sequences with known pan/zoom/rotation/reflection, brush-like content changes, missing frames, blank intervals and reacquisition. Feed the real estimator/fusion pipeline and assert MASTER.md's measurable targets where applicable, confidence loss on unobservable inputs and no confident wrong parity. Include landmark ground truth independent of the estimator. Do not fabricate a dataset from private art or tune tolerances merely to pass. Build/test; report error distributions and failing scenarios. Mark synthetic success separately from real CSP acceptance.
```

## 26 — Integration fault handling

```text
Implement step 26 under the Shared execution contract. Scope: src/app/main.cpp, src/capture/CaptureSession.cpp, src/graphics/DeviceResources.cpp, src/tracking/TrackingSession.cpp, tests/unit/TrackingSessionTests.cpp. Inspect existing ownership and implement the smallest missing target-close, document/layout invalidation, capture-loss, device-removal and shutdown recovery paths. Invalidate generations before teardown, stop callbacks safely, rebuild dependent resources in order, reapply affinity to any new private HWND and require fresh calibration/evidence before showing it. If this exceeds the five-file scope, finish one fault path and list the next substep. Build/test and inject lifecycle failures where supported; manually close/reopen CSP, resize repeatedly and exit while capturing. Distinguish simulated device loss from real driver reset evidence. Do not invoke destructive system-level GPU resets.
```

## 27 — Known-transform Windows fixture

```text
Implement step 27 under the Shared execution contract. Scope: tests/harness/CanvasFixture.h/.cpp, tests/harness/FixtureMain.cpp, tests/harness/README.md, CMakeLists.txt. Create a separate-process Win32 test canvas with an asymmetric synthetic document and explicit pan/zoom/rotation/flip controls that exposes known ground truth through a local test-only output file. Include mouse/pen input counters and controllable window/client geometry. Use it to validate capture-to-client mapping, DPI movement and measured registration error, not to replace CSP/OBS testing. Keep production discovery explicitly separate from the fixture opt-in mode; if that wiring needs a sixth file, split it into a later buildable substep. Build/test and record fixture measurements with sample counts and hardware.
```

## 28 — End-to-end acceptance and handoff

```text
Execute step 28 under the Shared execution contract. Do not add features. Inspect MASTER.md's gates and complete VALIDATION.md with actual evidence; make only focused defect fixes, each within five files and separately built/tested. Run clean Debug and Release configure/build/test from fresh ignored build directories using the documented presets or explicitly recorded binary-directory overrides. Confirm tests are discovered. Perform a 30-minute CSP drawing/navigation session, monitor age/error/memory trends, exercise pen/mixed-DPI/focus/modal/layout/document-switch and recovery cases, and repeat the saved OBS Display Capture exclusion test after final changes. Record hardware/software versions, artifact paths, measured p50/p95/max and every limitation. Mark inaccessible manual checks NOT RUN and required failures BLOCKED; do not label the prototype accepted while a mandatory gate is missing. Produce a short handoff describing how to launch, calibrate, record safely on the tested configuration, recover tracking and reproduce tests. No packaging, cloud upload or publication.
```

## Focused repair prompt — use when a gate fails

```text
Apply the Shared execution contract. Diagnose only the most recent failing gate in project-docs/VALIDATION.md. Reproduce it first and identify the earliest failing boundary. Change at most five implementation/config/test files plus the validation record. Preserve working behavior and avoid new frameworks or unrelated refactors. Add a regression test only where it can meaningfully reproduce the failure; keep hardware-specific checks manual. Run Debug configure/build/tests and the exact failing manual scenario if possible. Report root cause, evidence, fix and remaining uncertainty. Do not advance the milestone or invent a pass when the required hardware/application is unavailable.
```
