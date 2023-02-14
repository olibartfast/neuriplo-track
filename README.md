# Multi object tracking

Repo created to test multi object trackers, starting with SORT with code based on refence below.

## Dependencies
Opencv, Yolov5 exported to onnx using Pytorch v1.11.

### To Run
./yoloSort --link=<path to video or stream> --labels=<path to label file> --model_path=<mpath to model binary>

# reference
https://github.com/david8862/keras-YOLOv3-model-set/tree/master/tracking/cpp_inference/yoloSort
