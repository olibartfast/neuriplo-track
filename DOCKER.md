# Docker Usage Guide

This guide explains how to build and run the Multi-Object Tracking project using Docker.

## Prerequisites

- Docker Engine 20.10 or later
- NVIDIA Docker runtime (for GPU support)
- NVIDIA GPU with CUDA support (recommended)

### Installing NVIDIA Docker Runtime

```bash
# Add NVIDIA package repositories
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | sudo tee /etc/apt/sources.list.d/nvidia-docker.list

# Install nvidia-docker2
sudo apt-get update
sudo apt-get install -y nvidia-docker2
sudo systemctl restart docker
```

## Building the Docker Image

Build the Docker image from the project root directory:

```bash
docker build -t vision-tracking:latest .
```

This will create a multi-stage Docker image with all necessary dependencies.

## Running the Container

### Basic Usage

```bash
docker run --rm --gpus all \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  vision-tracking:latest \
  --link=/app/videos/your-video.mp4 \
  --tracker=BoTSORT \
  --labels=/app/coco.names \
  --model_path=/app/models/your-model.onnx \
  --class=car,person
```

### Using Docker Compose

Edit the `docker-compose.yml` file to set your desired parameters, then run:

```bash
docker-compose up
```

### With Display Output (X11 Forwarding)

To display video output on your host system:

```bash
xhost +local:docker

docker run --rm --gpus all \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  vision-tracking:latest \
  --link=/app/videos/your-video.mp4 \
  --tracker=BoTSORT \
  --labels=/app/coco.names \
  --model_path=/app/models/your-model.onnx \
  --class=car,person

xhost -local:docker
```

## Available Trackers

- `SORT` - Simple Online and Realtime Tracking
- `ByteTrack` - ByteTrack algorithm
- `BoTSORT` - BoT-SORT with ReID

## Volume Mounts

- `/app/models` - Mount your model files here
- `/app/videos` - Mount your video files here
- `/app/config` - BoTSORT configuration files

## Environment Variables

- `NVIDIA_VISIBLE_DEVICES` - Control which GPUs are visible (default: all)
- `NVIDIA_DRIVER_CAPABILITIES` - NVIDIA driver capabilities (default: compute,utility,video)

## Example Commands

### SORT Tracker
```bash
docker run --rm --gpus all \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  vision-tracking:latest \
  --link=/app/videos/input.mp4 \
  --tracker=SORT \
  --labels=/app/coco.names \
  --model_path=/app/models/yolo11x.onnx \
  --class=car,person,truck
```

### ByteTrack Tracker
```bash
docker run --rm --gpus all \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  vision-tracking:latest \
  --link=/app/videos/input.mp4 \
  --tracker=ByteTrack \
  --labels=/app/coco.names \
  --model_path=/app/models/yolo11x.onnx \
  --class=car,person
```

### BoTSORT Tracker with Custom Config
```bash
docker run --rm --gpus all \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  -v $(pwd)/trackers/BoTSORT/config:/app/config \
  vision-tracking:latest \
  --link=/app/videos/input.mp4 \
  --tracker=BoTSORT \
  --labels=/app/coco.names \
  --model_path=/app/models/yolo11x.onnx \
  --tracker_config=/app/config/tracker.ini \
  --gmc_config=/app/config/gmc.ini \
  --reid_config=/app/config/reid.ini \
  --reid_onnx=/app/models/reid.onnx \
  --class=person
```

### Using IP Camera Stream
```bash
docker run --rm --gpus all \
  -v $(pwd)/models:/app/models \
  vision-tracking:latest \
  --link=rtsp://192.168.1.100:554/stream \
  --tracker=ByteTrack \
  --labels=/app/coco.names \
  --model_path=/app/models/yolo11x.onnx \
  --class=person
```

## Troubleshooting

### GPU Not Detected
```bash
# Verify NVIDIA Docker is working
docker run --rm --gpus all nvidia/cuda:12.2.0-base-ubuntu22.04 nvidia-smi
```

### Permission Errors
If you encounter permission errors with mounted volumes:
```bash
# Run with user permissions
docker run --rm --gpus all \
  --user $(id -u):$(id -g) \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  vision-tracking:latest [...]
```

### Display Issues
If X11 forwarding doesn't work:
```bash
# Check DISPLAY variable
echo $DISPLAY

# Allow X11 connections
xhost +local:docker
```

## CPU-Only Version

If you don't have a GPU or want to run on CPU only, use the dedicated CPU Dockerfile:

### Building the CPU Image

```bash
docker build -f Dockerfile.cpu -t vision-tracking:cpu .
```

### Running with CPU Only

```bash
docker run --rm \
  -v $(pwd)/models:/app/models \
  -v $(pwd)/videos:/app/videos \
  vision-tracking:cpu \
  --link=/app/videos/your-video.mp4 \
  --tracker=BoTSORT \
  --labels=/app/coco.names \
  --model_path=/app/models/your-model.onnx \
  --class=car,person
```

### Using Docker Compose (CPU)

```bash
docker-compose -f docker-compose.cpu.yml up
```

**Note:** CPU inference will be significantly slower than GPU inference, especially for real-time video processing.

## Additional Notes

- The container runs as root by default. For production, consider running as a non-root user.
- Model files are not included in the image. You must mount them via volumes.
- Output files will be created in the same directory as the input video.
