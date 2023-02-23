# Multi object tracking

Repo created to test multi object trackers, starting with SORT with code based on refence below.

## Dependencies
Opencv, Yolov5 exported to onnx using Pytorch v1.11.

### To Run
```
./multi_object_tracking --link=<path to video or stream> --labels=<path to label file> --model_path=<path to model binary> --class=<class to track>
```

# reference
https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort

### TO DO
to try other trackers (i.e MHR, Online Multi-Object Tracking (OMOT), Joint Probabilistic Data Association (JPDA), Sequential Monte Carlo (SMC), Adaptive Background Mixture Model (ABM))?
