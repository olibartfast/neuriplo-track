# Multi-Object Tracking

Repo created to wrap and test multiobject trackers, starting with SORT, BoTSORT and ByteTrack with code based on references below. ByteTrack is include directly via FetchContent command, in BoTSORT project I replaced original ReId TensorRT model inference backend with Onnx-runtime.

## Dependencies
### Core
Opencv, Eigen3, glog, Onnx-runtime(for Botsort Re-Identification)
### Fetched
[Object Detection Inference Library](https://github.com/olibartfast/object-detection-inference) for object detection:
```
include(FetchContent)
FetchContent_Declare(
object-detection-inference
GIT_REPOSITORY https://github.com/olibartfast/object-detection-inference.git
GIT_TAG        master  # or the specific tag/branch you want to use
)
FetchContent_MakeAvailable(object-detection-inference)
```
### To run
```
./multi_object_tracking --link=<path to video or stream> --tracker=<tracking algorithm i.e. "SORT", "ByteTrack", "BoTSORT"> --labels=<path to label file> --model_path=<path to model binary> --class=<id of class to track> 
```
* Check [.vscode folder for examples](.vscode/launch.json)
# References
* SORT: https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort
* ByteTrack: https://github.com/Vertical-Beach/ByteTrack-cpp
* BoTSORT: https://github.com/viplix3/BoTSORT-cpp

### TO DO
 try other tracker libraries or custom porting/implementation 
