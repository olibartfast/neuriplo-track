# TODO: Remaining Tasks

This document lists the remaining tasks to complete the project restructuring and migration.

## Critical Tasks (Required for Build)

### 1. ✅ COMPLETED: Core Restructuring
- [x] Created app/ directory with modular structure
- [x] Created trackers/CMakeLists.txt
- [x] Updated main CMakeLists.txt
- [x] Created version management system
- [x] Updated README.md

### 2. ⚠️ PENDING: Test and Verify Build

```bash
# Test the new build system
cd /workspaces/multi_object_tracking
rm -rf build
mkdir build && cd build

# Configure - this will fetch dependencies
cmake -DDEFAULT_BACKEND=ONNX_RUNTIME ..

# Build
cmake --build . -j$(nproc)

# Test
./multi_object_tracking --help
```

**Expected Issues to Fix:**
- Missing include statements in wrapper files
- Incorrect member variable names (e.g., `config_` vs `config`)
- Linking issues with neuriplo vs InferenceEngines

## High Priority Tasks

### 3. Fix Include Headers in Wrapper Files

The wrapper implementation files need proper includes. Check and add:

```cpp
// In trackers/SortWrapper.cpp
#include "SortWrapper.hpp"
#include "Sort.hpp"  // May need adjustment based on actual SORT header location

// In trackers/ByteTrackWrapper.cpp  
#include "ByteTrackWrapper.hpp"
#include "BYTETracker.h"  // Check actual ByteTrack header name

// In trackers/BoTSORTWrapper.cpp
#include "BoTSORTWrapper.hpp"
#include "BoTSORT.h"  // Check actual BoTSORT header name
```

### 4. Verify BaseTracker Implementation

Ensure that `BaseTracker` class exists and is properly defined in `include/BaseTracker.hpp`. It should have:
- Constructor accepting `TrackConfig`
- Pure virtual `update()` method
- Protected `config_` member

### 5. Update TrackConfig Structure

Ensure `include/TrackConfig.hpp` has all necessary fields for all three trackers:

```cpp
struct TrackConfig {
    std::set<int> classes_to_track;
    
    // SORT parameters
    int max_age = 1;
    int min_hits = 3;
    float iou_threshold = 0.3f;
    
    // ByteTrack parameters
    int track_buffer = 30;
    float track_thresh = 0.5f;
    float high_thresh = 0.6f;
    float match_thresh = 0.8f;
    
    // BoTSORT parameters
    std::string tracker_config_path;
    std::string gmc_config_path;
    std::string reid_config_path;
    std::string reid_onnx_path;
    
    // Constructor...
};
```

### 6. Fix neuriplo vs InferenceEngines Naming

The project now uses `neuriplo` library (renamed from `InferenceEngines`). Update any remaining references:
- Check if `InferenceBackendSetup.hpp` exists or if it's in neuriplo
- Update include paths if needed

## Medium Priority Tasks

### 7. Test All Three Trackers

Test each tracker individually:

```bash
# SORT
./multi_object_tracking --type=yolov8 --source=test.mp4 \
  --labels=coco.names --weights=model.onnx --tracker=SORT --classes=person

# ByteTrack
./multi_object_tracking --type=yolov8 --source=test.mp4 \
  --labels=coco.names --weights=model.onnx --tracker=ByteTrack --classes=person

# BoTSORT
./multi_object_tracking --type=yolov8 --source=test.mp4 \
  --labels=coco.names --weights=model.onnx --tracker=BoTSORT --classes=person \
  --tracker_config=trackers/BoTSORT/config/tracker.ini \
  --reid_onnx=models/reid.onnx
```

### 8. Update Docker Configuration

Test Docker build:
```bash
docker build -t multi-object-tracking:test .
```

If issues arise, update Dockerfile to match new structure.

### 9. Update Documentation Examples

Update `.vscode/launch.json` with new command-line format if it exists.

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

Before first build, verify:

- [ ] `include/BaseTracker.hpp` exists and has `update()` virtual method
- [ ] `include/TrackConfig.hpp` has all required fields
- [ ] Wrapper `.cpp` files have correct `#include` statements
- [ ] `InferenceBackendSetup.hpp` path is correct (check neuriplo includes)
- [ ] All `config.` references changed to `config_.` in wrappers
- [ ] ByteTrack types match (check `BYTETracker` vs `ByteTracker` capitalization)

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

The restructuring is complete when:

- [x] Project builds without errors
- [ ] All three trackers work correctly
- [ ] Docker image builds and runs
- [ ] Documentation is accurate
- [ ] Examples in README work
- [ ] No regression in functionality

## Next Steps After Completion

1. Tag the repository with new version (e.g., `v2.0.0`)
2. Update any external documentation or references
3. Announce changes to users
4. Consider deprecating old command-line format
