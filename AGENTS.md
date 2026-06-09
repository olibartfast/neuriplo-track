# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project Overview

**neuriplo-track** is a C++20 multi-object tracking framework that combines detection (via [neuriplo](https://github.com/olibartfast/neuriplo) + [neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks)) with tracking algorithms SORT, ByteTrack, and BoTSORT.

## Repository Layout

```
app/           CLI application (neuriplo-track binary)
trackers/      Tracker implementations and wrappers
include/       Public headers (BaseTracker, TrackConfig, TrackedObject, wrappers)
cmake/         CMake modules (versions, dependency validation)
docs/          Architecture and algorithm documentation
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

- **BaseTracker** (`include/BaseTracker.hpp`): abstract interface; `update(detections, frame)` returns `TrackedObject` vectors.
- **Wrappers** (`include/*Wrapper.hpp`, `trackers/*Wrapper.cpp`): adapt SORT, ByteTrack, BoTSORT to the common interface.
- **TrackConfig** (`include/TrackConfig.hpp`): classes to track, IoU/age thresholds, BoTSORT config paths (tracker.ini, gmc.ini, reid.ini, reid ONNX).
- **MultiObjectTrackingApp** (`app/`): wires detector (`neuriplo_tasks::TaskFactory`), inference engine, and tracker; tracker creation lives in `MultiObjectTrackingApp::createTracker`.

BoTSORT requires a frame for Re-ID features; SORT and ByteTrack do not.

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

CI (`.github/workflows/ci.yml`): Release build with Ninja on ubuntu-24.04; separate job with `WERROR=ON`. Markdown-only changes do not trigger CI.

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
4. Extend CLI help / `CommandLineParser` if new options are required.
5. Update `docs/Tracking_Algorithms.md` and README examples if user-facing.
