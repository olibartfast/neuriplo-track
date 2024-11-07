# Multi-Object Tracking

Repo created to wrap and test multiobject trackers, starting with SORT, ByteTrack and BoTSORT with code based on references below.

## Dependencies
Opencv, Eigen3,  basic Yolov5 exported to onnx for detection task.

### To run
```
./multi_object_tracking --link=<path to video or stream> --tracker=<tracking algorithm i.e. "SORT", "ByteTrack" or "BoTSORT"> --labels=<path to label file> --model_path=<path to model binary> --class=<id of class to track> 
```

# References
* SORT: https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort
* ByteTrack: https://github.com/Vertical-Beach/ByteTrack-cpp
* BoTSORT: https://github.com/viplix3/BoTSORT-cpp

### TO DO
 try other tracker libraries or custom porting/implementation 
