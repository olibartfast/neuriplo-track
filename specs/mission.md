# Mission

## Pitch

**neuriplo-track** is a C++20 multi-object tracking framework that pairs a
pluggable detection stack with production-grade tracking algorithms (SORT,
ByteTrack, BoTSORT) behind one stable interface, so a tracking pipeline can be
retargeted to a different model or a different inference backend without
rewriting the application.

## Problem

Building a real-time MOT pipeline in C++ usually means gluing together three
moving parts by hand:

- **Detector models** change constantly (YOLOv4 → YOLOv26, RT-DETR, D-FINE), and
  each has its own pre/post-processing.
- **Inference backends** differ per deployment target — OpenCV DNN on a laptop,
  TensorRT on a Jetson, OpenVINO on Intel edge hardware, ONNX Runtime on a
  server.
- **Tracker implementations** are published as standalone research repos with
  incompatible APIs, data types, and configuration formats.

The result is a pipeline welded to one model, one backend, and one tracker. Any
change means touching application code.

## Solution

Three boundaries, each of which can be swapped independently:

| Boundary | Mechanism | What varies behind it |
|----------|-----------|-----------------------|
| Detection | `neuriplo_tasks::TaskFactory` | Model family and its pre/post-processing |
| Inference | `neuriplo`, selected by `DEFAULT_BACKEND` | OpenCV DNN, ONNX Runtime, TensorRT, LibTorch, OpenVINO, LibTensorFlow |
| Tracking | `BaseTracker` + per-algorithm wrappers | SORT, ByteTrack, BoTSORT, OC-SORT, C-BIoU |

The application (`MultiObjectTrackingApp`) only knows about
`neuriplo_tasks::Detection` in, `TrackedObject` out. Adding a tracker means
adding a wrapper, one branch in `createTracker`, and one entry in
`trackers/CMakeLists.txt` (the five-step procedure in `AGENTS.md`) — no changes
to the detection or inference path.

The tracker layer builds standalone (`BUILD_ONLY_LIB=ON`), so it can be embedded
in another application without pulling in the CLI or a detection backend.

## Users

- **CV / robotics engineers** prototyping a tracking pipeline who need to compare
  trackers on the same detections without rewiring the pipeline each time.
- **Edge deployment engineers** who develop against one backend and ship on
  another (TensorRT, OpenVINO) and want the switch to be a CMake flag.
- **Researchers** evaluating detector/tracker combinations, who need the two
  axes to vary independently to isolate what actually changed.
- **Integrators** who want only the `trackers` library and already have their own
  detection source.

## Non-Goals

- **Not a training framework.** Models are consumed as exported weights
  (`.onnx`, engine files); training and export happen elsewhere.
- **Not a detector implementation.** Detection lives in `neuriplo-tasks`; this
  repo consumes `neuriplo_tasks::Detection` and does not define parallel
  detection types.
- **Not a new tracking algorithm.** The value is integration and a stable
  interface over published algorithms, not novel tracking research.
- **Not a service.** The deliverable is a CLI and a library, not a REST/gRPC
  server or a video management system.

## Principles

1. **The interface is the product.** Tracker changes stay behind `BaseTracker`
   and the wrapper layer; trackers never couple directly to the app.
2. **One detection type.** `neuriplo_tasks::Detection` flows through; parallel
   types are introduced only with an explicit adapter.
3. **Backends are configuration, not code.** Switching inference backends is a
   CMake option, never an application-level edit.
4. **Reproducible dependencies.** Every fetched dependency is pinned through
   `versions.env`; no floating references in build files.
5. **Small, aligned diffs.** Changes match the surrounding style and stay scoped
   to their layer.
