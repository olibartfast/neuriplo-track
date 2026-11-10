# Multi-stage Dockerfile for Multi-Object Tracking Project
# Stage 1: Build environment
FROM nvidia/cuda:13.4.1-devel-ubuntu24.04 AS builder

# ONNX Runtime pin. Keep in sync with ONNXRUNTIME_VERSION in versions.env
# (matches neuriplo's own pin). Since 1.28 the GPU build is published per
# CUDA major as onnxruntime-linux-x64-gpu_cuda<MAJOR>-<ver>.
ARG ONNXRUNTIME_VERSION=1.28.0
ARG ONNXRUNTIME_GPU_ARCHIVE=onnxruntime-linux-x64-gpu_cuda13-1.28.0

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    ca-certificates \
    libopencv-dev \
    libeigen3-dev \
    libgoogle-glog-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install ONNX Runtime with GPU support
WORKDIR /tmp
RUN wget https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_GPU_ARCHIVE}.tgz \
    && tar -xzf ${ONNXRUNTIME_GPU_ARCHIVE}.tgz \
    && mv ${ONNXRUNTIME_GPU_ARCHIVE} /opt/onnxruntime \
    && rm ${ONNXRUNTIME_GPU_ARCHIVE}.tgz

# Set environment variables for ONNX Runtime
ENV ONNXRUNTIME_DIR=/opt/onnxruntime
ENV LD_LIBRARY_PATH=/opt/onnxruntime/lib:${LD_LIBRARY_PATH}

# Copy project files
WORKDIR /workspace
COPY . .

# Build the project
RUN rm -rf build && \
    cmake -G Ninja -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DDEFAULT_BACKEND=ONNX_RUNTIME \
    && cmake --build build --config Release

# Stage 2: Runtime environment
FROM nvidia/cuda:13.4.1-runtime-ubuntu24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies (Ubuntu 24.04 t64 package names)
RUN apt-get update && apt-get install -y \
    libopencv-core406t64 \
    libopencv-imgproc406t64 \
    libopencv-highgui406t64 \
    libopencv-video406t64 \
    libopencv-videoio406t64 \
    libgoogle-glog0v6t64 \
    libeigen3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy ONNX Runtime from builder
COPY --from=builder /opt/onnxruntime /opt/onnxruntime

# Set environment variables
ENV ONNXRUNTIME_DIR=/opt/onnxruntime
ENV LD_LIBRARY_PATH=/opt/onnxruntime/lib:${LD_LIBRARY_PATH}

# Create working directory
WORKDIR /app

# Copy the built executable and necessary files from builder
COPY --from=builder /workspace/build/neuriplo-track /app/
COPY --from=builder /workspace/coco.names /app/
COPY --from=builder /workspace/trackers/BoTSORT/config /app/config/

# Create directories for models and data
RUN mkdir -p /app/models /app/data /app/labels

# Set the entrypoint
ENTRYPOINT ["/app/neuriplo-track"]

# Default command (can be overridden)
CMD ["--help"]
