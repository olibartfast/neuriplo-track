# Feature Requirements — mask-conditioned association

Roadmap phase: [Phase 5 — Capability](../roadmap.md), the "Let masks reach the
tracker" item, plus the mask half of the "Remaining trackers" item.

## Goal

When the detector is a segmentation model, its masks reach the tracker and can
be used as an association cue instead of box overlap alone. Two objects whose
boxes overlap heavily but whose masks are disjoint — a pedestrian passing in
front of another, a player occluding a teammate — become separable without a
Re-ID model.

This is the prerequisite for any mask-based tracking in this repository,
including a future McByte.

## In Scope

- **Masks reach the tracker.**
  - `BaseTracker` gains an `update` overload taking
    `std::vector<neuriplo_tasks::InstanceSegmentation>`, with a default
    implementation that slices each element to its `Detection` base and
    delegates to the existing overload. Every current wrapper keeps working with
    no change and simply ignores masks.
  - `MultiObjectTrackingApp::processVideo` extracts `InstanceSegmentation`
    results from the task result variant (today only `Detection` is kept, so a
    segmentation model's masks are silently discarded) and calls the mask
    overload when any are present.
- **A mask IoU utility** (`trackers/common/MaskOverlap.{hpp,cpp}`): intersection
  over union of two binary masks, computed over the intersection of their
  bounding boxes so cost is proportional to object size, not frame size.
- **An opt-in mask cue in OC-SORT and C-BIoU**, controlled by one weight `w`:
  - association score becomes `(1 - w) * boxIoU + w * maskIoU` when both sides
    carry a mask, and plain box IoU otherwise, so a mixed or mask-less stream
    degrades gracefully;
  - a track's mask is the mask of its **last observation**, stored cropped to
    its bounding box;
  - `w = 0` (the default) reproduces today's behaviour exactly.
- **CLI**: `--mask_iou_weight` (default 0), following the existing pattern of
  optional overrides resolved in `makeTrackConfig`.
- **Tests**: mask IoU against hand-computed values; the slicing default; a
  scripted crossing where box overlap is ambiguous and only the masks resolve
  the identity correctly.
- **Documentation**: `docs/Tracking_Algorithms.md`, `README.md`, `AGENTS.md`,
  `specs/tech-stack.md`, `specs/roadmap.md`.

## Out of Scope

- **McByte.** Its cue is a *temporally propagated* mask: SAM seeds a tracklet's
  mask from its box, Cutie carries it forward frame to frame, so the mask holds
  identity independently of the detector. Nothing here propagates anything — the
  mask is whatever the detector produced this frame. No document in this branch
  may describe this feature as McByte or as an implementation of it.
- **A SAM or Cutie task.** Both belong in `neuriplo-tasks`, not here.
- **Mask-based visualisation.** Masks are used for association only; drawing
  still uses boxes.
- **Masks in BoTSORT, SORT, or ByteTrack.** The cue is added to the two trackers
  this project owns outright.
- **`TrackedObject` carrying a mask.** The output type stays a box.
- **Per-class association pools**, still one pool as before.

## Decisions

1. **Overload with a slicing default, not a changed signature.** Changing
   `BaseTracker::update` to take `InstanceSegmentation` would force every
   wrapper and caller to change and would push a segmentation type through a
   detection-only pipeline. An overload whose default slices keeps
   mission.md principle 2 ("one detection type") intact —
   `InstanceSegmentation` *is* a `Detection` — and leaves every existing tracker
   untouched.
   One ergonomic cost, found while building: `update({})` with a braced empty
   list no longer compiles, because the overload set cannot deduce which vector
   is meant. Call sites must name the type. Only the test suite was affected.
2. **Masks are stored cropped to the bounding box.** Every `neuriplo-tasks`
   postprocessor emits a full-frame single-channel 0/255 mask
   (`InstanceSegmentation::mask`, sized `mask_height` × `mask_width` = frame
   size), but its foreground is confined to the bounding box. Retaining
   full-frame masks for every live track would cost megabytes per track at
   1080p; cropping to the box is lossless for all current producers.
3. **`mask` is the source, not `mask_data`.** Only the RF-DETR postprocessor
   fills `mask_data`; the YOLO and EdgeCrafter paths leave it empty and populate
   `mask` alone. `neuriplo_tasks::toCvMat(const ImageMatrix &)` converts it.
4. **One weight, not a mode switch.** A single `[0, 1]` blend keeps the default
   bit-identical to current behaviour, lets masks be leaned on progressively,
   and avoids a second association code path.
5. **Foreground is any non-zero pixel.** Masks arrive binarised at 0/255; no
   further thresholding parameter is introduced.
6. **Detections without masks are not penalised.** If either side of a pair
   lacks a mask, that pair falls back to box IoU rather than scoring zero, so a
   detector that masks only some classes still tracks the rest.
7. **Fetched targets are built position independent.** Discovered during
   implementation: `libneuriplo-tasks.a` was compiled without `-fPIC`, and
   `MaskOverlap.cpp` is the first code in this repository to reference
   `ImageMatrix`, which pulls `image_matrix.cpp.o` into `libtrackers.so`'s link
   and fails it. `CMAKE_POSITION_INDEPENDENT_CODE ON` is set at the top of the
   root `CMakeLists.txt`. This is a latent build defect the feature exposed
   rather than caused: any use of the mask type from the shared library would
   have hit it.

## Constraints and Context

- `trackers` must keep building with `BUILD_ONLY_LIB=ON`, so the mask utility
  lives under `trackers/` and depends only on OpenCV and `neuriplo-tasks`.
- Must build clean under `-Wall -Wextra -Wpedantic -Werror`, match
  `.clang-format`, and add no dependency.
- Existing tracker behaviour must stay bit-identical at the default weight; the
  test suite from the previous branch is the regression signal.
- Per-frame cost matters: mask IoU runs inside the association loop, hence the
  bounding-box-limited computation in decision 2.
