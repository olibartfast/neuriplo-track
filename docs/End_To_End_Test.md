# End-to-End Test: EdgeCrafter + BoTSORT (Re-ID) on Video

Reproducible, step-by-step record of running the full pipeline:
**EdgeCrafter detector → BoTSORT tracker with Re-ID → annotated MKV output**, on a YouTube
clip, using the ONNX Runtime **GPU** backend.

The run tracked the **person** class on a 168 s (4204-frame) video with detector confidence
`0.5`, producing `assets/output.mkv`.

---

## 0. Prerequisites / environment

| Component | Version / Path |
|-----------|----------------|
| Tracker framework | this repo (`neuriplo-track`) |
| `neuriplo-tasks` | **v0.5.0** (sibling checkout at `../neuriplo-tasks`) |
| `neuriplo` | sibling checkout at `../neuriplo` |
| Inference backend | ONNX Runtime **1.19.2 (GPU)** — `~/dependencies/onnxruntime-linux-x64-gpu-1.19.2` |
| CUDA runtime libs | CUDA 12 + cuDNN 9.5 from pip `nvidia-*-cu12` packages |
| GPU | NVIDIA RTX 3060 Laptop (driver 580) |
| Eigen | header-only, installed to `~/.local` (no sudo) |
| Build | CMake ≥ 3.20, Ninja, C++20 |

EdgeCrafter is a multi-input, RT-DETR-style detector. Its ONNX has two inputs
(`images [1,3,640,640]`, `orig_target_sizes [1,2]`) and three outputs
(`labels [1,300]`, `boxes [1,300,4]`, `scores [1,300]`). It is end-to-end (DETR family),
so **no NMS** is applied — only score thresholding.

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

## 2. Provide Eigen (without sudo)

BoTSORT includes `<eigen3/Eigen/Core>`. The system `libeigen3-dev` was not installed and there
was no passwordless sudo. Install Eigen header-only into a local prefix from any existing Eigen
source tree (e.g. a fetched `eigen-src`):

```bash
cmake -S /path/to/eigen-src -B /tmp/eigen-build \
      -DCMAKE_INSTALL_PREFIX=$HOME/.local \
      -DBUILD_TESTING=OFF -DEIGEN_BUILD_DOC=OFF
cmake --install /tmp/eigen-build
# -> ~/.local/include/eigen3/Eigen/Core
# -> ~/.local/share/eigen3/cmake/Eigen3Config.cmake
```

---

## 3. Configure & build (ONNX Runtime backend)

```bash
cd /home/oli/repos/neuriplo-track
rm -rf build
export ONNXRUNTIME_DIR=$HOME/dependencies/onnxruntime-linux-x64-1.19.2   # for linking
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DDEFAULT_BACKEND=ONNX_RUNTIME \
      -DEigen3_DIR=$HOME/.local/share/eigen3/cmake \
      -DCMAKE_PREFIX_PATH=$HOME/.local
cmake --build build -j"$(nproc)"
```

The binary links `libonnxruntime.so.1` (CPU package, same 1.19.2 ABI). The **GPU** package is
swapped in at runtime via `LD_LIBRARY_PATH` (next steps), which is how the Ort CUDA provider
becomes available without rebuilding.

Output: `build/app/neuriplo-track`.

---

## 4. Export the Re-ID ONNX (OSNet-x0_25)

BoTSORT's Re-ID stage expects an ONNX with a single input `images [1,3,256,128]` and a single
output `output [1,512]` (see `trackers/BoTSORT/config/reid.ini`). None existed on disk, so it
was exported locally with `torchreid`:

```bash
pip install --user --break-system-packages torchreid gdown tensorboard onnxscript
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

> Note: these are **ImageNet-pretrained** OSNet weights (the only ones the library downloads
> without a hand-picked URL). The pipeline is fully functional; drop in an MSMT17-trained ONNX
> via `--reid_onnx` for stronger person discrimination.

---

## 5. Gather the detector and labels

- Detector: `~/repos/edgecrafter-cpp-inference/models/ecdet_s.onnx` (model type `ecdet` /
  `edgecrafter` → `EdgeCrafterDetection`).
- Labels: repo-root `coco.names` (`person` is class id 0).

---

## 6. Acquire the video

`yt-dlp` from apt was outdated (YouTube broke it). Upgrade and download
(video id `bjT7ECeCKO0`; the `?is=` share param is a typo of `?si=`):

```bash
pip install --user --break-system-packages --upgrade yt-dlp    # -> 2026.06.09
mkdir -p assets && cd assets
yt-dlp -f "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best" \
       --merge-output-format mp4 -o "video.%(ext)s" \
       "https://youtu.be/bjT7ECeCKO0"
# -> video.mp4 (4204 frames, 25 fps, 168 s, 682x480, AV1)
```

---

## 7. Run the pipeline on GPU

The Ort CUDA provider needs cuDNN 9 / cuBLAS 12 / cuDART 12, which are provided by the pip
`nvidia-*-cu12` packages in a local venv. Prepend all of them (plus the GPU Ort package and the
build's shared libs) to `LD_LIBRARY_PATH`:

```bash
cd /home/oli/repos/neuriplo-track
ORT=$HOME/dependencies/onnxruntime-linux-x64-gpu-1.19.2
NVLIB=$HOME/gpu_computing_venv/lib/python3.12/site-packages/nvidia
LDP="$ORT/lib"
for d in cublas cuda_runtime cudnn cufft curand cusolver cusparse \
         nvjitlink nccl nvtx cuda_nvrtc cuda_cupti cusparselt; do
    LDP="$LDP:$NVLIB/$d/lib"
done
LDP="$LDP:$PWD/build/_deps/neuriplo-build:$PWD/build/trackers:$PWD/build/_deps/bytetrack-build"
export LD_LIBRARY_PATH="$LDP:$LD_LIBRARY_PATH"
export GLOG_logtostderr=1

./build/app/neuriplo-track \
  --type=ecdet \
  --weights=$HOME/repos/edgecrafter-cpp-inference/models/ecdet_s.onnx \
  --source=$PWD/assets/video.mp4 \
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
Processing complete. Total frames: 4204
Output saved to: /home/oli/repos/neuriplo-track/assets/output.mkv
```

| | |
|---|---|
| Output | `assets/output.mkv` |
| Container/codec | Matroska / mpeg4 (mp4v fourcc) |
| Resolution / fps | 682x480 @ 25 fps |
| Duration | 168.16 s |
| Size | ~92 MB |
| Throughput | ~7-8 fps on RTX 3060 (EdgeCrafter + per-detection Re-ID) |

---

## Appendix — configuration notes

- **Detector confidence**: `--min_confidence` (default `0.25`); raised to `0.5` here. This is the
  EdgeCrafter postprocessor's score filter (no NMS — DETR family is end-to-end).
- **Tracker thresholds** (in `trackers/BoTSORT/config/tracker.ini`, separate from the detector):
  `track_high_thresh=0.6`, `track_low_thresh=0.1`, `new_track_thresh=0.7`, `match_thresh=0.7`,
  `appearance_thresh=0.25`, `lambda=0.985`. Re-ID is on by default (`enable_reid=true`); GMC off.
- **Re-ID config** (`reid.ini`): input `[1,3,256,128]`, output `[1,512]`, `swapRB=true`,
  `euclidean` distance. Input/output layer names in the ini are advisory — `ReID.cpp` reads the
  actual names from the ONNX at runtime.
- **CPU fallback**: drop `--use-gpu`, point `LD_LIBRARY_PATH` Ort entry at
  `~/dependencies/onnxruntime-linux-x64-1.19.2/lib`, and expect ~0.85 s/frame (~60 min for this
  video). For a quick CPU smoke test, trim first: `ffmpeg -i video.mp4 -t 12 -c:v libx264 -an clip.mp4`.
