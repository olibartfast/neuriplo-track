# Multi object tracking

Repo created to test multi object trackers, starting with SORT and ByteTrack with code based on refences below.

## Dependencies
Opencv, basic Yolov5 exported to onnx for detection task.

### To Run
```
./multi_object_tracking --link=<path to video or stream> --tracker=<tracking algorithm i.e. ``SORT`` or ``ByteTrack``> --labels=<path to label file> --model_path=<path to model binary> --class=<id of class to track>
```

# references
* Sort Tracker: https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort
* ByteTrack: https://github.com/Vertical-Beach/ByteTrack-cpp

### TO DO
 try other trackers libraries, or custom porting/implementations 