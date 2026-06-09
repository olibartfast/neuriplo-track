# Vision Tracking

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/std/the-standard)

C++ framework for multi-object tracking, integrating state-of-the-art tracking algorithms (SORT, ByteTrack, BoTSORT) with the [neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks) and [neuriplo](https://github.com/olibartfast/neuriplo) libraries for real-time object detection and tracking.

## Key Features

- **Multiple Tracking Algorithms**: SORT, ByteTrack, and BoTSORT
- **Switchable Inference Backends**: OpenCV DNN, ONNX Runtime, TensorRT, LibTorch, OpenVINO (via [neuriplo](https://github.com/olibartfast/neuriplo))
- **Multiple Detection Models**: YOLO series (v4-v12), RT-DETR, D-FINE, and more
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
- `--tracker`: Tracking algorithm (SORT, ByteTrack, BoTSORT)
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

### Help
```bash
./neuriplo-track --help
```

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
│   ├── *Wrapper.cpp/hpp     # Tracker wrappers
│   └── CMakeLists.txt
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

## Support

- Open an [issue](https://github.com/olibartfast/neuriplo-track/issues) for bug reports or feature requests
- Contributions, corrections, and suggestions are welcome


