# Multi-Object Tracking

This repository is designed to wrap and test multiple object trackers, beginning with SORT, BoTSORT, and ByteTrack. The core code is adapted from the referenced implementations below. ByteTrack is integrated directly using the `FetchContent` command in `CMakeLists`. For the BoTSORT project, the original ReID TensorRT model inference backend has been replaced with an ONNX Runtime implementation.

## Dependencies

### Core
- OpenCV
- Eigen3
- glog
- ONNX Runtime (for BoTSORT Re-Identification)

### Fetched
- [Object Detection Inference Library](https://github.com/olibartfast/object-detection-inference) for object detection:

```cmake
include(FetchContent)
FetchContent_Declare(
  object-detection-inference
  GIT_REPOSITORY https://github.com/olibartfast/object-detection-inference.git
  GIT_TAG        master  # or the specific tag/branch you want to use
)
FetchContent_MakeAvailable(object-detection-inference)
```

## Running the Tracker

```bash
./multi_object_tracking --link=<path to video or stream> --tracker=<tracking algorithm i.e., "SORT", "ByteTrack", "BoTSORT"> --labels=<path to label file> --model_path=<path to model binary> --class=<id of class to track>
```

*For examples, check the [.vscode folder](.vscode/launch.json).*

## References
- [SORT](https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort)
- [ByteTrack](https://github.com/Vertical-Beach/ByteTrack-cpp)
- [BoTSORT](https://github.com/viplix3/BoTSORT-cpp)

### To-Do
- Explore additional tracker libraries or custom implementations.
