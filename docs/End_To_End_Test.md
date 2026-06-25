# End-to-End Test: EdgeCrafter + BoTSORT (Re-ID) on Video

Reproducible, step-by-step guide for running the full pipeline:
**EdgeCrafter detector → BoTSORT tracker with Re-ID → annotated video output**, using the
ONNX Runtime **GPU** backend (CPU fallback noted at the end).

The reference run tracked the **person** class at detector confidence `0.5` and produced an
annotated MKV. Adjust paths/models to your own assets.

> Commands below assume you are at the repo root and use the environment variables defined in
> the **Setup** block. Fill them in for your machine before running.

---

## 0. Prerequisites

| Component | Requirement |
|-----------|-------------|
| Tracker framework | this repo (`neuriplo-track`) |
| `neuriplo-tasks` | **v0.5.0** (sibling checkout, `../neuriplo-tasks`) |
| `neuriplo` | sibling checkout, `../neuriplo` |
| Inference backend | ONNX Runtime **1.19.2** (GPU build for CUDA EP; CPU build also works) |
| GPU (optional) | any CUDA-capable NVIDIA GPU + matching CUDA 12 / cuDNN 9 runtime |
| Eigen | system `libeigen3-dev`, or a header-only install (see step 2) |
| Build tools | CMake ≥ 3.20, Ninja, C++20 |

EdgeCrafter is a multi-input, RT-DETR-style detector. Its ONNX has two inputs
(`images [1,3,640,640]`, `orig_target_sizes [1,2]`) and three outputs
(`labels [1,300]`, `boxes [1,300,4]`, `scores [1,300]`). It is end-to-end (DETR family), so
**no NMS** is applied — only score thresholding.

### Setup (fill in for your environment)

```bash
# repo root
REPO=$(pwd)

# ONNX Runtime 1.19.2 — CPU package used to LINK, GPU package used to RUN (same ABI)
ORT_LINK_DIR=$HOME/path/to/onnxruntime-linux-x64-1.19.2          # has headers + libonnxruntime.so
ORT_GPU_DIR=$HOME/path/to/onnxruntime-linux-x64-gpu-1.19.2       # CUDA-provider build (optional)

# your assets
ECDET_ONNX=$HOME/path/to/ecdet_s.onnx        # EdgeCrafter detection model
VIDEO=$REPO/assets/video.mp4                 # any input video
```

---

## 1. Sync the dependency pin to neuriplo-tasks v0.5.0

`versions.env`:

```diff
-NEURIPLO_TASKS_VERSION="master"
+NEURIPLO_TASKS_VERSION="v0.5.0"
```

The v0.4.0 rename (`vision-core → neuriplo-tasks`) and v0.5.0 EdgeCrafter batch support are
already reflected in this repo's API usage, so no source changes are required — only the pin.

---

## 2. Provide Eigen

BoTSORT includes `<eigen3/Eigen/Core>`. Preferred:

```bash
sudo apt install -y libeigen3-dev
```

No-sudo fallback — install Eigen header-only into a local prefix from any Eigen source tree:

```bash
EIGEN_SRC=/path/to/eigen-src
cmake -S "$EIGEN_SRC" -B /tmp/eigen-build \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local \
      -DBUILD_TESTING=OFF -DEIGEN_BUILD_DOC=OFF
cmake --install /tmp/eigen-build
EIGEN_DIR=$HOME/.local/share/eigen3/cmake   # used in step 3
```

---

## 3. Configure & build (ONNX Runtime backend)

```bash
cd "$REPO"
rm -rf build
export ONNXRUNTIME_DIR=$ORT_LINK_DIR                 # for linking (headers + import lib)
EIGEN_ARG=()
[ -n "${EIGEN_DIR:-}" ] && EIGEN_ARG=(-DEigen3_DIR="$EIGEN_DIR" -DCMAKE_PREFIX_PATH=$HOME/.local)

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DDEFAULT_BACKEND=ONNX_RUNTIME "${EIGEN_ARG[@]}"
cmake --build build -j"$(nproc)"
```

The binary links `libonnxruntime.so.1` (1.19.2 ABI). The **GPU** package can be swapped in at
runtime via `LD_LIBRARY_PATH` (step 7) to enable the CUDA provider without rebuilding.

Output: `build/app/neuriplo-track`.

---

## 4. Export the Re-ID ONNX (OSNet-x0_25)

BoTSORT's Re-ID stage expects an ONNX with a single input `images [1,3,256,128]` and a single
output `output [1,512]` (see `trackers/BoTSORT/config/reid.ini`). If you don't already have one,
export it with `torchreid`:

```bash
pip install --user torchreid gdown tensorboard onnxscript   # add --break-system-packages if needed
```

```python
# export_reid.py
import warnings; warnings.filterwarnings("ignore")
import torch
from torchreid import models

net = models.build_model(name="osnet_x0_25", num_classes=1000)  # loads ImageNet weights
net.eval()
dummy = torch.randn(1, 3, 256, 128)        # (N,C,H,W) = 256 tall, 128 wide
with torch.no_grad():
    assert tuple(net(dummy).shape) == (1, 512)

torch.onnx.export(net, dummy, "assets/osnet_x0_25.onnx",
                  input_names=["images"], output_names=["output"], opset_version=13)
```

Result: `assets/osnet_x0_25.onnx` (~0.8 MB), verified `images [1,3,256,128] → output [1,512]`.

> These are **ImageNet-pretrained** OSNet weights (the only ones the library downloads without a
> hand-picked URL). The pipeline is fully functional; drop in an MSMT17-trained ONNX via
> `--reid_onnx` for stronger person discrimination.

---

## 5. Gather the detector and labels

- **Detector**: an EdgeCrafter detection ONNX (e.g. `ecdet_s.onnx`). Model type `ecdet` /
  `edgecrafter` routes to `EdgeCrafterDetection` in `neuriplo-tasks`.
- **Labels**: repo-root `coco.names` (`person` is class id 0).

---

## 6. Acquire the video

Any video works. To pull one from YouTube, use a recent `yt-dlp` (older versions break against
current YouTube):

```bash
pip install --user --upgrade yt-dlp
mkdir -p assets
yt-dlp -f "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best" \
       --merge-output-format mp4 -o "assets/video.%(ext)s" \
       "<your-video-url>"
```

---

## 7. Run the pipeline on GPU

The Ort CUDA provider needs cuDNN 9 / cuBLAS 12 / cuDART 12. Provide them from wherever you have
them (a CUDA toolkit install, or pip `nvidia-*-cu12` packages), then prepend the GPU Ort package
and the build's shared libs to `LD_LIBRARY_PATH`:

```bash
cd "$REPO"

# Example: CUDA libs from pip nvidia-*-cu12 packages — adjust to your layout
NVLIB=$HOME/path/to/python/site-packages/nvidia
LDP="$ORT_GPU_DIR/lib"
for d in cublas cuda_runtime cudnn cufft curand cusolver cusparse \
         nvjitlink nccl nvtx cuda_nvrtc cuda_cupti cusparselt; do
    [ -d "$NVLIB/$d/lib" ] && LDP="$LDP:$NVLIB/$d/lib"
done
LDP="$LDP:$PWD/build/_deps/neuriplo-build:$PWD/build/trackers:$PWD/build/_deps/bytetrack-build"
export LD_LIBRARY_PATH="$LDP:$LD_LIBRARY_PATH"
export GLOG_logtostderr=1

./build/app/neuriplo-track \
  --type=ecdet \
  --weights=$ECDET_ONNX \
  --source=$VIDEO \
  --labels=$PWD/coco.names \
  --tracker=BoTSORT \
  --reid_onnx=$PWD/assets/osnet_x0_25.onnx \
  --classes=person \
  --use-gpu=true \
  --min_confidence=0.5 \
  --output=$PWD/assets/output.mkv
```

Verification in the log:
- `ORTInfer.cpp:229] Using CUDA GPU` (detection runs on the CUDA EP; ReID auto-selects CUDA too).
- Inputs `images 1x3x640x640` + `orig_target_sizes 1x2`; outputs `labels/boxes/scores 1x300`.
- `ReID model` initialised, `GMC disabled`.

For long runs, launch detached so the process survives the shell:
`setsid ./build/app/neuriplo-track ... < /dev/null > assets/run.log 2>&1 &`

---

## 8. Result

```
Processing complete. Total frames: <N>
Output saved to: .../assets/output.mkv
```

| | |
|---|---|
| Output | `assets/output.mkv` (Matroska / mpeg4 — `mp4v` fourcc) |
| Resolution / fps | same as source |
| Throughput | GPU: a few-to-several fps (EdgeCrafter + per-detection Re-ID); CPU: ~1 fps |

---

## Appendix — configuration notes

- **Detector confidence**: `--min_confidence` (default `0.25`); raised to `0.5` in the reference
  run. This is the EdgeCrafter postprocessor's score filter (no NMS — DETR family is end-to-end).
- **Tracker thresholds** (in `trackers/BoTSORT/config/tracker.ini`, separate from the detector):
  `track_high_thresh=0.6`, `track_low_thresh=0.1`, `new_track_thresh=0.7`, `match_thresh=0.7`,
  `appearance_thresh=0.25`, `lambda=0.985`. Re-ID is on by default (`enable_reid=true`); GMC off.
- **Re-ID config** (`reid.ini`): input `[1,3,256,128]`, output `[1,512]`, `swapRB=true`,
  `euclidean` distance. Input/output layer names in the ini are advisory — `ReID.cpp` reads the
  actual names from the ONNX at runtime.
- **CPU fallback**: drop `--use-gpu` and point the `LD_LIBRARY_PATH` Ort entry at the CPU package
  `lib/` dir. For a quick CPU smoke test, trim the source first:
  `ffmpeg -i video.mp4 -t 12 -c:v libx264 -an clip.mp4`.
