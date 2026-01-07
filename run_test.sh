#!/bin/bash

# Multi-Object Tracking Test Script
# This script runs a test with YOLOv5 nano model and a sample video

set -e

# Configuration
MODEL_PATH="yolov5n.onnx"
VIDEO_PATH="test_video.mp4"
LABELS_PATH="coco.names"
DETECTOR_TYPE="yolov5"
TRACKER="${TRACKER:-SORT}"
CLASSES="car,person"
INPUT_SIZES="640,640"
EXECUTABLE="./build/vision-tracking"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}Multi-Object Tracking Test Script${NC}"
echo "=================================="

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}Error: Executable not found at $EXECUTABLE${NC}"
    echo "Please build the project first with: cd build && make"
    exit 1
fi

# Check if model exists
if [ ! -f "$MODEL_PATH" ]; then
    echo -e "${YELLOW}Model not found. Exporting YOLOv5n from torch hub...${NC}"
    
    # Create virtual environment if it doesn't exist
    if [ ! -d "venv" ]; then
        echo -e "${YELLOW}Creating virtual environment...${NC}"
        python3 -m venv venv
    fi
    
    # Activate virtual environment and export model
    source venv/bin/activate
    
    python3 << 'EOF'
import sys
import subprocess

try:
    # Install required packages
    print("Installing required packages...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "-q", "torch", "torchvision", "ultralytics", "onnx", "onnxscript", "pandas", "PyYAML", "scipy", "seaborn", "tqdm", "Pillow", "matplotlib"])
    
    import torch
    
    # Load YOLOv5n model from torch hub
    print("Loading YOLOv5n model from torch hub...")
    model = torch.hub.load('ultralytics/yolov5', 'yolov5n', pretrained=True, trust_repo=True)
    
    # Export to ONNX with fixed input size
    print("Exporting to ONNX...")
    model.model.eval()
    dummy_input = torch.randn(1, 3, 640, 640)
    torch.onnx.export(
        model.model,
        dummy_input,
        "yolov5n.onnx",
        input_names=['images'],
        output_names=['output0'],
        opset_version=12,
        dynamic_axes=None
    )
    print("Model exported successfully to yolov5n.onnx")
except Exception as e:
    print(f"Error exporting model: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
EOF
    
    EXPORT_STATUS=$?
    deactivate
    
    if [ $EXPORT_STATUS -ne 0 ]; then
        echo -e "${RED}Failed to export model from torch hub${NC}"
        exit 1
    fi
fi

# Check if test video exists
if [ ! -f "$VIDEO_PATH" ]; then
    echo -e "${YELLOW}Test video not found. Downloading sample video...${NC}"
    wget -q --show-progress -O test_video.mp4 "https://github.com/intel-iot-devkit/sample-videos/raw/master/car-detection.mp4"
elif [ ! -s "$VIDEO_PATH" ] || ! ffprobe -v error "$VIDEO_PATH" &>/dev/null; then
    echo -e "${YELLOW}Test video is invalid. Re-downloading...${NC}"
    rm -f "$VIDEO_PATH"
    wget -q --show-progress -O test_video.mp4 "https://github.com/intel-iot-devkit/sample-videos/raw/master/car-detection.mp4"
fi

# Check if labels file exists
if [ ! -f "$LABELS_PATH" ]; then
    echo -e "${RED}Error: Labels file not found at $LABELS_PATH${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Running inference...${NC}"
echo "Model: $MODEL_PATH"
echo "Video: $VIDEO_PATH"
echo "Tracker: $TRACKER"
echo "Classes: $CLASSES"
echo "Input sizes: $INPUT_SIZES"
echo ""

# Run the tracking
# Check which CLI interface the executable uses (old run.cpp vs new app/)
if $EXECUTABLE --help 2>&1 | grep -q "^\s*--source"; then
    # New app/ interface
    $EXECUTABLE \
        --type=$DETECTOR_TYPE \
        --source=$VIDEO_PATH \
        --labels=$LABELS_PATH \
        --weights=$MODEL_PATH \
        --tracker=$TRACKER \
        --classes=$CLASSES \
        --input_sizes=$INPUT_SIZES || true
else
    # Old run.cpp interface
    $EXECUTABLE \
        --detector_type=$DETECTOR_TYPE \
        --link=$VIDEO_PATH \
        --labels=$LABELS_PATH \
        --tracker=$TRACKER \
        --classes=$CLASSES \
        --model_path=$MODEL_PATH \
        --input_sizes=$INPUT_SIZES || true
fi

# Check if output was generated
OUTPUT_FILE="test_video_processed.mp4"
if [ -f "$OUTPUT_FILE" ]; then
    OUTPUT_SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
    echo ""
    echo -e "${GREEN}✓ Success!${NC}"
    echo "Output generated: $OUTPUT_FILE ($OUTPUT_SIZE)"
else
    echo -e "${RED}✗ Error: Output file not generated${NC}"
    exit 1
fi
