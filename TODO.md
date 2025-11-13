# TODO: Remaining Tasks

This document lists the remaining tasks to complete the project restructuring and migration.

## Critical Tasks (Required for Build)

### 1. ✅ COMPLETED: Core Restructuring
- [x] Created app/ directory with modular structure
- [x] Created trackers/CMakeLists.txt
- [x] Updated main CMakeLists.txt
- [x] Created version management system
- [x] Updated README.md

### 2. ✅ COMPLETED: Build System Verification

Successfully built with:
```bash
cd /workspaces/multi_object_tracking
mkdir build && cd build
cmake -DDEFAULT_BACKEND=ONNX_RUNTIME ..
cmake --build . -j$(nproc)
```

**Build Results:**
- ✅ multi_object_tracking executable (295KB)
- ✅ libtrackers.so library
- ✅ All dependencies resolved (OpenCV 4.6.0, glog, Eigen3, ONNX Runtime 1.19.2)

## High Priority Tasks

### 3. ✅ COMPLETED: Fix Include Headers in Wrapper Files

All wrapper implementation files now have correct includes:
- ✅ SortWrapper.cpp - Uses `TrackingBox` from Sort.hpp
- ✅ ByteTrackWrapper.cpp - Uses `byte_track::BYTETracker` and `byte_track::Object`
- ✅ BoTSORTWrapper.cpp - Uses `botsort::BoTSORT` and `botsort::Detection`

### 4. ✅ COMPLETED: BaseTracker Implementation

BaseTracker properly defined in `include/BaseTracker.hpp`:
- ✅ Constructor accepting `TrackConfig`
- ✅ Pure virtual `update()` method
- ✅ Protected `config_` member

### 5. ✅ COMPLETED: Update TrackConfig Structure

`include/TrackConfig.hpp` now includes all parameters:
- ✅ SORT: max_age, min_hits, iou_threshold
- ✅ ByteTrack: track_buffer, track_thresh, high_thresh, match_thresh
- ✅ BoTSORT: tracker_config_path, gmc_config_path, reid_config_path, reid_onnx_path

### 6. ✅ COMPLETED: neuriplo Integration

Successfully integrated neuriplo library:
- ✅ InferenceBackendSetup from neuriplo
- ✅ ModelInfo includes added to app CMakeLists
- ✅ Detector creation uses neuriplo DetectorSetup

## Medium Priority Tasks

### 7. ⚠️ PENDING: Test All Three Trackers

Test each tracker individually with actual video/model files:

```bash
cd /workspaces/multi_object_tracking/build

# SORT
./app/multi_object_tracking --type=yolov8 --source=test.mp4 \
  --labels=../coco.names --weights=model.onnx --tracker=SORT --classes=0

# ByteTrack  
./app/multi_object_tracking --type=yolov8 --source=test.mp4 \
  --labels=../coco.names --weights=model.onnx --tracker=ByteTrack --classes=0

# BoTSORT
./app/multi_object_tracking --type=yolov8 --source=test.mp4 \
  --labels=../coco.names --weights=model.onnx --tracker=BoTSORT --classes=0 \
  --tracker_config=../trackers/BoTSORT/config/tracker.ini \
  --reid_onnx=reid.onnx
```

**Note**: Requires actual test video and model files.

### 8. ⚠️ PENDING: Update Docker Configuration

Test Docker build with new structure:
```bash
docker build -t multi-object-tracking:test .
```

May need to update Dockerfile paths and build steps.

### 9. ⚠️ PENDING: Update Documentation Examples

- Update `.vscode/launch.json` if it exists
- Verify all README examples work
- Update any tutorial documentation

## Low Priority Tasks

### 10. Add Unit Tests

Consider adding unit tests:
- Test CommandLineParser
- Test tracker creation
- Test utility functions

### 11. Add CI/CD Pipeline

Create GitHub Actions or similar for:
- Build testing
- Docker image building
- Documentation deployment

### 12. Performance Benchmarks

Add benchmarking capabilities:
- FPS measurements
- Tracking accuracy metrics
- Memory usage profiling

## Quick Fix Checklist

All items completed! ✅

- [x] `include/BaseTracker.hpp` exists and has `update()` virtual method
- [x] `include/TrackConfig.hpp` has all required fields  
- [x] Wrapper `.cpp` files have correct `#include` statements
- [x] `InferenceBackendSetup.hpp` path is correct (from neuriplo)
- [x] All `config.` references changed to `config_.` in wrappers
- [x] ByteTrack types correct (`byte_track::BYTETracker`)
- [x] BoTSORT types correct (`botsort::BoTSORT`, `botsort::Detection`)
- [x] SORT types correct (`TrackingBox` from Sort.hpp)
- [x] ONNX Runtime includes and libraries added for BoTSORT
- [x] Detectors library building correctly

## Known Issues from Old Code

From the original `run.cpp`, these need verification:

1. **Detection filtering logic** - Ensure it matches in `MultiObjectTrackingApp::drawDetections()`
2. **Video writer codec** - Verify `'m', 'p', '4', 'v'` codec works, may need adjustment
3. **Color generation** - RNG seeding for consistent colors across runs
4. **Frame skipping** - Old code showed every frame, new code can skip with display option

## Getting Help

If you encounter issues:

1. **Build errors**: Check CMake output carefully for missing dependencies
2. **Include errors**: Verify header locations in fetched dependencies
3. **Link errors**: Ensure all required libraries are linked in CMakeLists.txt
4. **Runtime errors**: Check command-line argument format and file paths

## Success Criteria

The restructuring progress:

- [x] Project builds without errors ✅
- [ ] All three trackers work correctly (needs runtime testing)
- [ ] Docker image builds and runs
- [ ] Documentation is accurate
- [ ] Examples in README work
- [ ] No regression in functionality

## Completed Work Summary

### Build System
- ✅ CMake configuration with FetchContent for dependencies
- ✅ object-detection-inference integration (detectors library)
- ✅ neuriplo integration (inference backends)
- ✅ ByteTrack-cpp integration via FetchContent
- ✅ Version management with versions.env

### Code Implementation
- ✅ BaseTracker abstract class with config_ member
- ✅ TrackConfig structure with all tracker parameters
- ✅ SortWrapper using SORT's TrackingBox API
- ✅ ByteTrackWrapper using byte_track namespace
- ✅ BoTSORTWrapper using botsort namespace with ReID support
- ✅ MultiObjectTrackingApp with detector and tracker integration
- ✅ CommandLineParser with all required options
- ✅ Utility functions for drawing and file handling

### Dependencies
- ✅ OpenCV 4.6.0
- ✅ glog (Google Logging)
- ✅ Eigen3
- ✅ ONNX Runtime 1.19.2
- ✅ neuriplo library (inference backends)
- ✅ VideoCapture library
- ✅ ByteTrack-cpp library

### Files Created/Modified
- ✅ app/main.cpp, app/src/*.cpp, app/inc/*.hpp
- ✅ trackers/SortWrapper.cpp, ByteTrackWrapper.cpp, BoTSORTWrapper.cpp
- ✅ trackers/CMakeLists.txt
- ✅ include/BaseTracker.hpp, TrackConfig.hpp, *Wrapper.hpp
- ✅ CMakeLists.txt (main project)
- ✅ cmake/versions.cmake
- ✅ README.md, docs/Migration_Guide.md, docs/Build_Instructions.md

## Next Steps After Completion

1. Tag the repository with new version (e.g., `v2.0.0`)
2. Update any external documentation or references
3. Announce changes to users
4. Consider deprecating old command-line format
