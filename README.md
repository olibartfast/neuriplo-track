# Vision Tracking

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/std/the-standard)

C++ framework for multi-object tracking, integrating state-of-the-art tracking algorithms (SORT, ByteTrack, BoTSORT, OC-SORT, C-BIoU) with the [neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks) and [neuriplo](https://github.com/olibartfast/neuriplo) libraries for real-time object detection and tracking.

## Key Features

- **Multiple Tracking Algorithms**: SORT, ByteTrack, BoTSORT, OC-SORT, and C-BIoU
- **Mask-Conditioned Association**: with a segmentation model, OC-SORT and C-BIoU can associate on mask overlap instead of box overlap
- **Switchable Inference Backends**: OpenCV DNN, ONNX Runtime, TensorRT, LibTorch, OpenVINO (via [neuriplo](https://github.com/olibartfast/neuriplo))
- **Multiple Detection Models**: YOLO series (v4->26), RT-DETR, D-FINE, and more
- **Modular Architecture**: Trackers library can be built independently
- **Docker Deployment Ready**: Container support for easy deployment
- **Fetched Dependencies**: Bundles [neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks) (common tasks/types), [neuriplo](https://github.com/olibartfast/neuriplo) (inference engine), and ByteTrack via FetchContent. All the fetched dependencies will be downloaded inside the `build/_deps` directory.

## Requirements

### Core Dependencies
- CMake (≥ 3.20)
- C++20 compiler (GCC ≥ 8.0)
- OpenCV (≥ 4.6)
  ```bash
  apt install libopencv-dev
  ```
- Google Logging (glog)
  ```bash
  apt install libgoogle-glog-dev
  ```
- Eigen3 (≥ 3.3)
  ```bash
  apt install libeigen3-dev
  ```

### Dependency Management

This project automatically fetches:
- **[neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks)**: Core computer vision tasks and types
- **[neuriplo](https://github.com/olibartfast/neuriplo)**: Unified neural inference interface
- **[ByteTrack-cpp](https://github.com/Vertical-Beach/ByteTrack-cpp)**: ByteTrack implementation

For inference backend setup (ONNX Runtime, TensorRT, etc.), refer to the [neuriplo setup guide](https://github.com/olibartfast/neuriplo#-requirements).

#### Quick Setup
```bash
# Install system dependencies
sudo apt update && sudo apt install -y libopencv-dev libgoogle-glog-dev libeigen3-dev

# Setup inference backend (if not using OpenCV DNN)
# See neuriplo documentation for backend-specific setup
```

## Building

### Build 
```bash
mkdir build && cd build
cmake -DDEFAULT_BACKEND=<backend> -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Inference Backend Options
Replace `<backend>` with:
- `OPENCV_DNN` (default, no additional setup required)
- `ONNX_RUNTIME`
- `TENSORRT`
- `LIBTORCH`
- `OPENVINO`
- `LIBTENSORFLOW`

See [neuriplo documentation](https://github.com/olibartfast/neuriplo) for backend setup details.

## Usage

### Command Line Options

```bash
./neuriplo-track \
  --type=<model_type> \
  --source=<input_source> \
  --labels=<labels_file> \
  --weights=<model_weights> \
  --tracker=<tracker_algorithm> \
  --classes=<classes_to_track> \
  [--use-gpu] \
  [--output=<output_path>] \
  [--display]
```

#### Required Parameters

- `--type`: Detector model type (yolov4-yolov12, rtdetr, dfine, etc.)
- `--source`: Input video file or stream URL
- `--labels`: Path to class labels file
- `--weights`: Path to model weights
- `--tracker`: Tracking algorithm (`SORT`, `ByteTrack`, `BoTSORT`, `OCSORT`, `CBIoU`; the paper spellings `OC-SORT` and `C-BIoU` are accepted too)
- `--classes`: Comma-separated list of classes to track (e.g., "person,car")

#### Optional Parameters

- `--use-gpu`: Enable GPU support (default: false)
- `--min_confidence`: Minimum confidence threshold (default: 0.25)
- `--batch`: Batch size for inference (default: 1)
- `--output`: Output video path (auto-generated if not specified)
- `--display`: Display output video in real-time (default: false)
- `--tracker_config`: Path to tracker config file (for BoTSORT)
- `--gmc_config`: Path to GMC config file (for BoTSORT)
- `--reid_config`: Path to ReID config file (for BoTSORT)
- `--reid_onnx`: Path to ReID ONNX model (for BoTSORT)
- `--input_sizes`: Input sizes for models with dynamic dimensions. Provide values only for dynamic dimensions in C,H,W order.
  - If the model has fixed channels (e.g., YOLO `1,3,-1,-1`), pass `H,W` (such as `640,640`).
  - If all dims are dynamic (e.g., `1,-1,-1,-1`), use `C,H,W` (such as `3,640,640`).
  - See `.vscode/launch.json` for concrete examples.

#### Tracker Tuning Parameters

Every parameter below is optional; leaving one out selects the default of the
tracker named by `--tracker`.

| Flag | Applies to | Default | Meaning |
|------|------------|---------|---------|
| `--max_age` | SORT, OCSORT, CBIoU | 1 (SORT), 30 (OCSORT/CBIoU) | Frames a track survives without a matching detection |
| `--min_hits` | SORT, OCSORT, CBIoU | 3 | Detections before a track is reported |
| `--iou_threshold` | SORT, OCSORT, CBIoU | 0.3 | Minimum overlap for an association |
| `--track_buffer` | ByteTrack | 30 | Frames a lost track is buffered |
| `--track_thresh` | ByteTrack | 0.5 | High/low detection split |
| `--high_thresh` | ByteTrack | 0.6 | Score required to start a track |
| `--match_thresh` | ByteTrack | 0.8 | Association threshold |
| `--delta_t` | OCSORT | 3 | Frame gap used to estimate direction of travel (OCM) |
| `--inertia` | OCSORT | 0.2 | Weight of the direction-consistency term |
| `--det_thresh` | OCSORT | 0.6 | Minimum detection score OC-SORT will track; note this gates *after* `--min_confidence` |
| `--biou_b1` | CBIoU | 0.3 | Buffer scale for the first matching round |
| `--biou_b2` | CBIoU | 0.5 | Buffer scale for the second round |
| `--motion_n` | CBIoU | 5 | Observations averaged by the motion model |
| `--mask_iou_weight` | OCSORT, CBIoU | 0 | Weight of mask overlap in association, 0–1. Needs a segmentation model; see [mask-conditioned association](docs/Tracking_Algorithms.md#mask-association) |

### Examples

#### Basic tracking with SORT
```bash
./neuriplo-track \
  --type=yolo \
  --source=video.mp4 \
  --labels=coco.names \
  --weights=yolov8n.onnx \
  --tracker=SORT \
  --classes=person,car
```

#### Advanced tracking with BoTSORT and GPU
```bash
./neuriplo-track \
  --type=yolo \
  --source=rtsp://camera_ip:port/stream \
  --labels=coco.names \
  --weights=yolo11x.onnx \
  --tracker=BoTSORT \
  --classes=person \
  --use-gpu \
  --tracker_config=trackers/BoTSORT/config/tracker.ini \
  --reid_onnx=models/reid.onnx \
  --display
```

#### ByteTrack with TensorRT
```bash
./neuriplo-track \
  --type=yolo \
  --source=video.mp4 \
  --labels=coco.names \
  --weights=yolov8n.engine \
  --tracker=ByteTrack \
  --classes=person,bicycle,car,motorcycle \
  --use-gpu
```

#### OC-SORT for objects that get occluded
```bash
./neuriplo-track \
  --type=yolo \
  --source=video.mp4 \
  --labels=coco.names \
  --weights=yolov8n.onnx \
  --tracker=OCSORT \
  --classes=person \
  --max_age=45 \
  --inertia=0.3
```

#### C-BIoU for fast or irregular motion
```bash
./neuriplo-track \
  --type=yolo \
  --source=video.mp4 \
  --labels=coco.names \
  --weights=yolov8n.onnx \
  --tracker=CBIoU \
  --classes=person,sports\ ball \
  --biou_b1=0.4 \
  --biou_b2=0.7
```

#### Segmentation model with the mask cue
```bash
./neuriplo-track \
  --type=yoloseg \
  --source=video.mp4 \
  --labels=coco.names \
  --weights=yolo11n-seg.onnx \
  --tracker=OCSORT \
  --classes=person \
  --mask_iou_weight=0.5
```

### Help
```bash
./neuriplo-track --help
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

The suite drives every tracker with scripted detection sequences, so it needs no
model weights and no video. Pass `-DNEURIPLO_TRACK_BUILD_TESTS=OFF` to skip
building it.

## Docker Deployment

### Building Image
```bash
docker build --rm -t neuriplo-track:latest -f Dockerfile .
```

### Running Container
```bash
docker run --gpus all --rm \
  -v $(pwd)/data:/app/data \
  -v $(pwd)/models:/models \
  -v $(pwd)/labels:/labels \
  neuriplo-track:latest \
  --type=yolov8 \
  --source=/app/data/video.mp4 \
  --labels=/labels/coco.names \
  --weights=/models/yolov8n.onnx \
  --tracker=ByteTrack \
  --classes=person,car \
  --use-gpu
```

## 📁 Project Structure

```
neuriplo-track/
├── app/                      # Application code
│   ├── inc/                  # Application headers
│   ├── src/                  # Application source files
│   ├── main.cpp             # Main entry point
│   └── CMakeLists.txt
├── trackers/                 # Tracking algorithms
│   ├── SORT/                # SORT implementation
│   ├── ByteTrack/           # ByteTrack (fetched)
│   ├── BoTSORT/             # BoTSORT implementation
│   ├── OCSORT/              # OC-SORT implementation
│   ├── CBIoU/               # C-BIoU implementation
│   ├── *Wrapper.cpp/hpp     # Tracker wrappers
│   └── CMakeLists.txt
├── tests/                    # ctest suite (no model or video required)
├── include/                  # Common headers
│   ├── BaseTracker.hpp
│   ├── TrackedObject.hpp
│   └── TrackConfig.hpp
├── cmake/                    # CMake modules
│   ├── versions.cmake       # Version management
│   └── DependencyValidation.cmake
├── versions.env             # Dependency versions
├── CMakeLists.txt           # Main build configuration
└── README.md

```

## Documentation

- [AGENTS.md](AGENTS.md) — guide for AI coding agents
- [System Architecture](docs/System_Architecture.md)
- [Tracking Algorithms](docs/Tracking_Algorithms.md)
- [Code Examples](docs/Code_Examples.md)
- [Build Instructions](docs/Build_Instructions.md)
- [Multiple Object Tracking Guide](docs/Multiple_Object_Tracking_Guide.md)

## Video Demo

[YOLO11x + BoTSORT Tracker Demo](https://www.youtube.com/watch?v=jYtL8RP6K3s)

## Acknowledgments
- [SORT](https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort) - Simple Online and Realtime Tracking
- [ByteTrack](https://github.com/Vertical-Beach/ByteTrack-cpp) - ByteTrack C++ implementation
- [BoTSORT](https://github.com/viplix3/BoTSORT-cpp) - BoTSORT C++ implementation
- [OC-SORT](https://arxiv.org/abs/2203.14360) - Observation-Centric SORT (implemented in-tree from the paper)
- [C-BIoU](https://arxiv.org/abs/2211.14317) - Cascaded Buffered IoU (implemented in-tree from the paper)

## Support

- Open an [issue](https://github.com/olibartfast/neuriplo-track/issues) for bug reports or feature requests
- Contributions, corrections, and suggestions are welcome


