# Build Instructions

## Prerequisites

### System Requirements
- Ubuntu 20.04+ or equivalent Linux distribution
- CMake 3.20 or higher
- C++20 compatible compiler (GCC 8.0+)
- CUDA 11.0+ (for GPU support)

### Required Dependencies
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    libopencv-dev \
    libgoogle-glog-dev \
    libeigen3-dev \
    pkg-config
```

## Building from Source

### Quick Start
```bash
# Clone repository
git clone https://github.com/olibartfast/vision-tracking.git
cd vision-tracking

# Check dependencies
./scripts/setup_dependencies.sh

# Build
mkdir build && cd build
cmake -DDEFAULT_BACKEND=ONNX_RUNTIME ..
cmake --build . -j$(nproc)
```

### Build Options

#### 1. Complete Build (Library + Application)
```bash
mkdir build && cd build
cmake -DDEFAULT_BACKEND=<backend> -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

#### 2. Library-Only Build
Build just the trackers library without the application:
```bash
mkdir build && cd build
cmake -DBUILD_ONLY_LIB=ON -DDEFAULT_BACKEND=<backend> ..
cmake --build . -j$(nproc)
```

#### 3. Debug Build
```bash
mkdir build && cd build
cmake -DDEFAULT_BACKEND=<backend> -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
```

### Backend Options

Replace `<backend>` with one of:

#### OpenCV DNN (Default)
```bash
cmake -DDEFAULT_BACKEND=OPENCV_DNN ..
```
- No additional setup required
- CPU-only inference
- Good for testing and development

#### ONNX Runtime
```bash
cmake -DDEFAULT_BACKEND=ONNX_RUNTIME ..
```
- Setup required: See [object-detection-inference docs](https://github.com/olibartfast/object-detection-inference)
- CPU and GPU support available

#### TensorRT
```bash
cmake -DDEFAULT_BACKEND=TENSORRT ..
```
- Setup required: See [object-detection-inference docs](https://github.com/olibartfast/object-detection-inference)
- Best performance on NVIDIA GPUs
- Requires TensorRT installation

#### LibTorch
```bash
cmake -DDEFAULT_BACKEND=LIBTORCH ..
```
- Setup required: See [object-detection-inference docs](https://github.com/olibartfast/object-detection-inference)
- PyTorch backend for inference

#### OpenVINO
```bash
cmake -DDEFAULT_BACKEND=OPENVINO ..
```
- Setup required: See [object-detection-inference docs](https://github.com/olibartfast/object-detection-inference)
- Optimized for Intel hardware

## Inference Backend Setup

For detailed instructions on setting up specific inference backends, refer to the [object-detection-inference documentation](https://github.com/olibartfast/object-detection-inference#-requirements).

### Quick Backend Setup
```bash
# ONNX Runtime
cd /path/to/object-detection-inference
./scripts/setup_dependencies.sh --backend onnx_runtime

# TensorRT
./scripts/setup_dependencies.sh --backend tensorrt

# LibTorch (CPU)
./scripts/setup_dependencies.sh --backend libtorch --compute-platform cpu

# LibTorch (GPU with CUDA)
./scripts/setup_dependencies.sh --backend libtorch --compute-platform cuda
```

## Docker Build

### Standard Build
```bash
docker build -t vision-tracking:latest .
```

### With Specific Backend
```bash
docker build -t vision-tracking:onnxruntime \
    --build-arg BACKEND=ONNX_RUNTIME .
```

## Verifying Build

### Check Executable
```bash
./build/vision-tracking --help
```

### Run Simple Test
```bash
./build/vision-tracking \
    --type=yolov8 \
    --source=/path/to/video.mp4 \
    --labels=coco.names \
    --weights=/path/to/model.onnx \
    --tracker=SORT \
    --classes=person
```

## Troubleshooting

### Common Issues

#### 1. CMake Cannot Find Dependencies
```bash
# Solution: Install missing packages
sudo apt install -y libopencv-dev libgoogle-glog-dev libeigen3-dev

# Or check CMake output for specific missing package
```

#### 2. Fetched Dependencies Fail
```bash
# Solution: Clear CMake cache and rebuild
rm -rf build
mkdir build && cd build
cmake ..
```

#### 3. Linking Errors
```bash
# Solution: Ensure inference backend is properly setup
# Follow backend-specific setup instructions from object-detection-inference
```

#### 4. CUDA/GPU Issues
```bash
# Check CUDA installation
nvcc --version
nvidia-smi

# Rebuild with GPU support
cmake -DDEFAULT_BACKEND=ONNX_RUNTIME ..
```

### Getting More Help

- Check [object-detection-inference issues](https://github.com/olibartfast/object-detection-inference/issues)
- Open an issue on this repository
- Review CMake output for specific error messages

## Advanced Build Options

### Custom Dependency Versions

Edit `versions.env` to specify custom versions:
```bash
# versions.env
OBJECT_DETECTION_INFERENCE_VERSION="v1.2.3"
BYTETRACK_VERSION="custom-branch"
```

### Using Local Dependencies

To use a local version of object-detection-inference:
```cmake
# In CMakeLists.txt, comment out FetchContent and add:
add_subdirectory(/path/to/object-detection-inference detectors)
```

### Parallel Builds
```bash
# Use all available cores
cmake --build . -j$(nproc)

# Or specify number of cores
cmake --build . -j4
```

## Next Steps

- Read [README.md](../README.md) for usage instructions
- Check [Migration_Guide.md](Migration_Guide.md) if upgrading from old version
- Review [Code_Examples.md](Code_Examples.md) for integration examples
