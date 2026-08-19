# Tech Stack

## Language & Toolchain

| Item | Version / Setting | Source |
|------|-------------------|--------|
| Language | C++20 (`CMAKE_CXX_STANDARD 20`, required) | `CMakeLists.txt` |
| Build system | CMake ≥ 3.20 | `versions.env` (`CMAKE_MIN_VERSION`) |
| Compiler | GCC ≥ 8.0 (MSVC supported in warning config) | `README.md`, `CMakeLists.txt` |
| Generator (CI) | Ninja | `.github/workflows/ci.yml` |
| Warnings | `-Wall -Wextra -Wpedantic` (`/W4` on MSVC); `WERROR=ON` adds `-Werror` / `/WX` | `CMakeLists.txt` |
| Compile DB | `CMAKE_EXPORT_COMPILE_COMMANDS ON` (clangd) | `CMakeLists.txt` |
| PIC | `CMAKE_POSITION_INDEPENDENT_CODE ON` — `trackers` is SHARED and links the fetched static `neuriplo-tasks` | `CMakeLists.txt` |

## System Dependencies

Minimum versions are declared in `versions.env` and enforced by
`cmake/DependencyValidation.cmake`.

| Dependency | Minimum | Role |
|------------|---------|------|
| OpenCV | 4.6.0 | Video I/O, image types, visualization, GMC/Re-ID support |
| glog | 0.6.0 | Application logging (`LOG(INFO)` / `LOG(ERROR)`) |
| Eigen3 | 3.3.0 | Linear algebra for Kalman filtering and matching |

```bash
sudo apt install -y build-essential cmake libopencv-dev libgoogle-glog-dev libeigen3-dev
```

## Fetched Dependencies

Pulled at configure time via `FetchContent` into `build/_deps/`. Versions are
read from `versions.env` by `cmake/versions.cmake`.

| Dependency | Pin | Role |
|------------|-----|------|
| [neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks) | `v0.8.0` | CV task layer and shared types (`Detection`, `TaskFactory`) |
| [neuriplo](https://github.com/olibartfast/neuriplo) | `master` | Unified inference interface across backends |
| [ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp) | `main` | ByteTrack implementation |

`neuriplo-tasks` and `neuriplo` are auto-detected as **sibling checkouts**: if
`../neuriplo-tasks` or `../neuriplo` exists with a `CMakeLists.txt`, CMake uses
the local tree instead of fetching (via `FETCHCONTENT_SOURCE_DIR_*`). Useful for
cross-repo development.

## Inference Backends

Selected at configure time with `-DDEFAULT_BACKEND=<backend>`; default in
`CMakeLists.txt` is `ONNX_RUNTIME`.

| Backend | Setup |
|---------|-------|
| `OPENCV_DNN` | None beyond OpenCV |
| `ONNX_RUNTIME` | Auto-downloads 1.19.2 (linux-x64-gpu) to `$HOME/dependencies/`, or set `ONNX_RUNTIME_DIR` |
| `TENSORRT` | Per [neuriplo](https://github.com/olibartfast/neuriplo#-requirements) |
| `LIBTORCH` | Per neuriplo |
| `OPENVINO` | Per neuriplo |
| `LIBTENSORFLOW` | Per neuriplo |

ONNX Runtime headers/libs are validated at configure time; the CUDA provider
(`onnxruntime_providers_cuda`) is optional and falls back to CPU.

## Vendored Tracker Sources

| Tracker | Location | Notes |
|---------|----------|-------|
| SORT | `trackers/SORT/` | Kalman tracker + Hungarian assignment, in-tree |
| BoTSORT | `trackers/BoTSORT/` | In-tree; Kalman (const-velocity and acceleration-based), `lapjv` matching, global motion compensation, ONNX Re-ID |
| OC-SORT | `trackers/OCSORT/` | In-tree, from [arXiv 2203.14360](https://arxiv.org/abs/2203.14360); observation-centric re-update, momentum-weighted association, last-observation recovery. Reuses `HungarianAlgorithm` and `cv::KalmanFilter` |
| Mask overlap | `trackers/common/` | Binary-mask IoU used as an optional association cue by OC-SORT and C-BIoU; consumes `neuriplo_tasks::InstanceSegmentation` masks |
| C-BIoU | `trackers/CBIoU/` | In-tree, from [arXiv 2211.14317](https://arxiv.org/abs/2211.14317); buffered IoU, two-round cascaded matching, mean-displacement motion model (no Kalman filter) |
| ByteTrack | fetched | Adapted through `ByteTrackWrapper` |

BoTSORT is configured through INI files in `trackers/BoTSORT/config/`:
`tracker.ini`, `gmc.ini`, `reid.ini`. It is the only tracker that requires the
frame (for Re-ID feature extraction). Every other tracker is configured entirely
from CLI flags resolved by `makeTrackConfig` (`app/src/utils.cpp`), which applies
per-algorithm defaults before applying command-line overrides.

## Build Targets & Options

| Option | Default | Effect |
|--------|---------|--------|
| `DEFAULT_BACKEND` | `ONNX_RUNTIME` | Inference backend selection |
| `CMAKE_BUILD_TYPE` | — | `Release` for deployment, `Debug` in the strict-warning CI job |
| `BUILD_ONLY_LIB` | `OFF` | Build the `trackers` shared library without the CLI app |
| `WERROR` | `OFF` | Treat warnings as errors |

Targets: `trackers` (SHARED library) and `neuriplo-track` (CLI executable, built
unless `BUILD_ONLY_LIB=ON`).

## Repository Layout

```
app/           CLI application (neuriplo-track binary)
  inc/         AppConfig, CommandLineParser, MultiObjectTrackingApp, utils
  src/         Implementations
trackers/      SORT/, BoTSORT/, *Wrapper.cpp
include/       BaseTracker, TrackConfig, TrackedObject, wrapper headers
cmake/         versions.cmake, DependencyValidation.cmake
docs/          Architecture, algorithms, build, MOT guide, e2e test
specs/         Product specs (this folder) and dated feature packets
tests/         ctest suite: trackers, track_config, utils
scripts/       setup_dependencies.sh, mot_docker_cpu.sh
```

## Core Types

| Type | Header | Role |
|------|--------|------|
| `BaseTracker` | `include/BaseTracker.hpp` | Abstract interface; `update(detections, frame)` → `TrackedObject` |
| `TrackConfig` | `include/TrackConfig.hpp` | Per-tracker parameters (SORT age/hits/IoU, ByteTrack buffer/thresholds, BoTSORT config paths) |
| `TrackedObject` | `include/TrackedObject.hpp` | Tracker output |
| `AppConfig` | `app/inc/AppConfig.hpp` | CLI-level config (source, weights, backend settings, output) |
| `neuriplo_tasks::Detection` | fetched | Detection type flowing into every tracker |

## Tooling

| Concern | Tool | Config |
|---------|------|--------|
| Tests | ctest + plain assertions (no framework) | `tests/CMakeLists.txt`, `tests/test_util.hpp` |
| Formatting | clang-format | `.clang-format` |
| Static analysis | clang-tidy | `.clang-tidy` |
| Pre-commit | pre-commit | `.pre-commit-config.yaml` |
| CI | GitHub Actions | `.github/workflows/ci.yml`, `lint.yml`, `.github/actions/setup-deps` |
| Debugging | VS Code | `.vscode/launch.json` |

CI runs on `ubuntu-24.04`: a Release/Ninja build with ccache plus a
Debug + `WERROR=ON` job. Markdown and `docs/**` changes are excluded from CI
triggers.

## Deployment

| Artifact | File | Notes |
|----------|------|-------|
| GPU image | `Dockerfile`, `docker-compose.yml` | Run with `--gpus all` |
| CPU image | `Dockerfile.cpu`, `docker-compose.cpu.yml` | No CUDA required |

Details in [`DOCKER.md`](../DOCKER.md).

## CLI Surface

```bash
./neuriplo-track \
  --type=<model_type> --source=<video|stream> --labels=<labels_file> \
  --weights=<model> --tracker=<SORT|ByteTrack|BoTSORT|OCSORT|CBIoU> --classes=<a,b> \
  [--use-gpu] [--output=<path>] [--display]
```

BoTSORT additionally needs `--tracker_config` and `--reid_onnx`, and usually
`--gmc_config` / `--reid_config`.

Tracker tuning flags, all optional and defaulting per algorithm: `--max_age`,
`--min_hits`, `--iou_threshold`, `--track_buffer`, `--track_thresh`,
`--high_thresh`, `--match_thresh`, `--delta_t`, `--inertia`, `--det_thresh`,
`--biou_b1`, `--biou_b2`, `--motion_n`, `--mask_iou_weight`.

## License

MIT ([`LICENSE`](../LICENSE)).
