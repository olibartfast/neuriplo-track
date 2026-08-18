# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project Overview

**neuriplo-track** is a C++20 multi-object tracking framework that combines detection (via [neuriplo](https://github.com/olibartfast/neuriplo) + [neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks)) with tracking algorithms SORT, ByteTrack, BoTSORT, OC-SORT, and C-BIoU.

## Repository Layout

```
app/           CLI application (neuriplo-track binary)
trackers/      Tracker implementations and wrappers
include/       Public headers (BaseTracker, TrackConfig, TrackedObject, wrappers)
cmake/         CMake modules (versions, dependency validation)
docs/          Architecture and algorithm documentation
specs/         Product specs: constitution (mission/tech-stack/roadmap) + dated feature packets; see specs/README.md
tests/         ctest suite; scripted detections only, no model or video needed
```

Fetched at configure time into `build/_deps/`: neuriplo-tasks, neuriplo, ByteTrack-cpp.

## Build

```bash
sudo apt install -y build-essential cmake libopencv-dev libgoogle-glog-dev libeigen3-dev

mkdir build && cd build
cmake -DDEFAULT_BACKEND=ONNX_RUNTIME -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

Inference backends: `OPENCV_DNN` (default, no extra setup), `ONNX_RUNTIME`, `TENSORRT`, `LIBTORCH`, `OPENVINO`, `LIBTENSORFLOW`. ONNX Runtime setup follows [neuriplo](https://github.com/olibartfast/neuriplo).

Optional flags: `BUILD_ONLY_LIB=ON` (trackers library only), `WERROR=ON` (strict warnings).

## Run / Verify

```bash
./build/neuriplo-track --help

./build/neuriplo-track \
  --type=yolo --source=video.mp4 --labels=coco.names \
  --weights=yolov8n.onnx --tracker=SORT --classes=person
```

## Architecture

- **BaseTracker** (`include/BaseTracker.hpp`): abstract interface; `update(detections, frame)` returns `TrackedObject` vectors. A second `update` overload takes `neuriplo_tasks::InstanceSegmentation`; its default slices to `Detection` and delegates, so only mask-aware trackers override it. Note `update({})` is ambiguous — name the vector type.
- **Mask cue** (`trackers/common/MaskOverlap.hpp`): mask IoU blended into association by `TrackConfig::mask_iou_weight` (`--mask_iou_weight`), implemented in OC-SORT and C-BIoU. Not McByte: masks are per-frame, never propagated.
- **Wrappers** (`include/*Wrapper.hpp`, `trackers/*Wrapper.cpp`): adapt SORT, ByteTrack, BoTSORT, OC-SORT, and C-BIoU to the common interface.
- **TrackConfig** (`include/TrackConfig.hpp`): classes to track, IoU/age thresholds, per-algorithm parameters (ByteTrack, OC-SORT, C-BIoU), BoTSORT config paths (tracker.ini, gmc.ini, reid.ini, reid ONNX).
- **makeTrackConfig / canonicalTrackerName** (`app/src/utils.cpp`): resolve `AppConfig` + CLI overrides into a `TrackConfig`, applying per-algorithm defaults; `--tracker` accepts `OC-SORT`/`C-BIoU` spellings.
- **MultiObjectTrackingApp** (`app/`): wires detector (`neuriplo_tasks::TaskFactory`), inference engine, and tracker; tracker creation lives in `MultiObjectTrackingApp::createTracker`.

BoTSORT requires a frame for Re-ID features; SORT, ByteTrack, OC-SORT, and C-BIoU do not.

## Code Conventions

- C++20, CMake ≥ 3.20.
- Match existing style: 2-space indent in headers, `clang-format` / `clang-tidy` configs in repo root.
- Use glog (`LOG(INFO)`, `LOG(ERROR)`) for application logging.
- Detections use `neuriplo_tasks::Detection`; do not introduce parallel detection types without a clear adapter.
- Keep tracker changes behind `BaseTracker` and the wrapper layer; avoid coupling trackers to the app directly.
- Minimize scope: prefer small, focused diffs aligned with surrounding patterns.

## Testing & CI

```bash
ctest --test-dir build --output-on-failure
```

Three suites: `trackers` (tracker behaviour against scripted `Detection`
sequences), `track_config` (CLI → `TrackConfig` resolution), `utils`. They need
no weights and no video. `-DNEURIPLO_TRACK_BUILD_TESTS=OFF` skips building them.
When adding tracker logic, add a case to `tests/test_trackers.cpp`.

CI (`.github/workflows/ci.yml`): Release build with Ninja on ubuntu-24.04; separate job with `WERROR=ON`. Markdown-only changes do not trigger CI.

## Hyperlink verification

When editing `README.md` or any documentation with hyperlinks:
- Verify all relative links resolve to existing files in the repo (`ls <path>`).
- Verify absolute GitHub URLs are reachable (use `curl -sI <url>` or a quick fetch).
- Prefer absolute GitHub blob/tree URLs over fragile cross-repo relative paths.

## Documentation

| Doc | Purpose |
|-----|---------|
| `README.md` | Usage, CLI flags, Docker |
| `docs/System_Architecture.md` | Design patterns, data structures |
| `docs/Tracking_Algorithms.md` | SORT / ByteTrack / BoTSORT concepts |
| `docs/Code_Examples.md` | Implementation snippets |
| `docs/Build_Instructions.md` | Detailed build and troubleshooting |
| `docs/Multiple_Object_Tracking_Guide.md` | MOT background and metrics |
| `DOCKER.md` | Container deployment |

## Common Pitfalls

- BoTSORT needs `--tracker_config`, `--reid_onnx`, and often `--gmc_config` / `--reid_config`; validate paths exist.
- `--input_sizes`: for fixed-channel models pass `H,W`; for fully dynamic models pass `C,H,W`.
- Fetched deps live under `build/_deps/`; clear `build/` if FetchContent resolution fails.
- Do not commit `build/`, credentials, or large model weights.

## When Adding a Tracker

1. Implement algorithm (or wrap external code) under `trackers/`.
2. Add wrapper implementing `BaseTracker`.
3. Register in `MultiObjectTrackingApp::createTracker` and `trackers/CMakeLists.txt`.
4. Extend CLI help / `CommandLineParser` if new options are required, add the
   parameters to `TrackConfig`, and give the algorithm its defaults in
   `makeTrackConfig`.
5. Add coverage in `tests/test_trackers.cpp`.
6. Update `docs/Tracking_Algorithms.md`, README examples, and `specs/roadmap.md`
   if user-facing.
