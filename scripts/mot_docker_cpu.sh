#!/bin/bash
# Build the CPU image
# docker build -f Dockerfile.cpu -t multi-object-tracking:cpu .

# Run without GPU
docker run --rm \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  multi-object-tracking:cpu \
  --link=/app/videos/input.mp4 \
  --tracker=SORT \
  --labels=/app/coco.names \
  --model_path=/app/models/yolo11x.onnx \
  --class=car,person