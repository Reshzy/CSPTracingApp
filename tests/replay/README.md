# Deterministic tracking replay (synthetic)

This suite feeds seeded 320x240 textured ROI sequences into the real `VisualTracker` + `TrackingSession` + `TransformFusion` pipeline. Landmark ground truth is generated from the known warp, not from the estimator. Fixtures are synthetic (LCG noise plus geometric marks). They are not private artwork and are not Clip Studio Paint frames.

## Run

From the repository root, using the documented CMake presets:

```powershell
$env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug --target TracingApp.TrackingReplayTests
ctest --preset windows-debug --output-on-failure -R TrackingReplayTests
```

Default seed is `0xC0FF` (the Prompt 17 combined-texture fixture). `BuildSequence(seed)` is deterministic for a given OpenCV/MSVC build.

## What passing means

GoogleTest PASS here is **synthetic replay** evidence: pan/zoom/rotation/reflection, brush-like strokes, missing frames, blank intervals, and reacquisition on generated pixels.

MASTER.md median ≤2 px / p95 ≤5 px / 0.5° / 0.5% checks are applied in ROI pixels on this 320x240 sequence, not on 1080p/1440p CSP. Observation-to-present age is not measured (no present path).

This does **not** certify real CSP tracking, pen behavior, or OBS Display Capture exclusion.
