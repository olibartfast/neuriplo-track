# Feature Plan — mask-conditioned association

## Group 1 — Mask primitive

1. `trackers/common/MaskOverlap.{hpp,cpp}`: a `MaskRegion` value type (a
   `cv::Mat` of `CV_8U` plus the frame-space origin of its top-left corner),
   a factory that crops a full-frame `neuriplo_tasks::InstanceSegmentation` mask
   to its bounding box, and `maskIoU(a, b)` computed over the intersection of
   the two regions.
2. Register the source and include directory in `trackers/CMakeLists.txt`.
3. `tests/test_mask_overlap.cpp`: hand-computed overlaps — identical, disjoint,
   half-overlapping, empty, and non-overlapping-bbox cases.

*Observable:* `ctest` covers mask IoU before anything consumes it.

## Group 2 — Masks reach the tracker

4. `BaseTracker`: add the `InstanceSegmentation` overload with the slicing
   default implementation.
5. `MultiObjectTrackingApp::processVideo`: collect `InstanceSegmentation`
   results alongside `Detection`, and call the mask overload when any are
   present; drawing keeps using the sliced detections.
6. `tests/test_trackers.cpp`: feeding `InstanceSegmentation` through the default
   overload yields the same tracks as feeding the sliced `Detection` values.

*Observable:* a segmentation model's masks arrive at the tracker; every existing
tracker behaves exactly as before.

## Group 3 — The cue

7. `TrackConfig::mask_iou_weight` (default 0), the `--mask_iou_weight` flag, its
   `AppConfig` override, and its resolution in `makeTrackConfig`.
8. OC-SORT: accept masks in `update`, keep each track's last-observation mask on
   `OCSortKalmanTracker`, and blend mask IoU into both the OCM cost and the OCR
   cost.
9. C-BIoU: the same, blended into both cascade rounds, with buffered box IoU as
   the fallback.
10. Wrappers: pass masks through `OCSortWrapper` / `CBIoUWrapper`, applying the
    same class filter as the detection path.

*Observable:* `--tracker=OCSORT --mask_iou_weight=0.5` runs end to end.

## Group 4 — Proof the cue works

11. `tests/test_trackers.cpp`: a scripted overlap where two objects' boxes
    coincide almost exactly but their masks are disjoint — assert that identity
    is preserved at `w = 1` for both trackers, and that `w = 0` reproduces the
    exact behaviour recorded before this branch.
12. `tests/test_track_config.cpp`: the new flag's default and override.

*Observable:* a failing-without-masks case that passes with masks.

## Group 5 — Documentation

13. `docs/Tracking_Algorithms.md`: a mask-conditioned association section,
    stating plainly that the mask is per-frame and not propagated, and how that
    differs from McByte.
14. `README.md` (flag table, a segmentation example), `AGENTS.md` (interface
    note), `specs/tech-stack.md`, `specs/roadmap.md`.

## Group 6 — Verification

15. Release build, Debug + `WERROR=ON` build, `BUILD_ONLY_LIB=ON` build.
16. Full `ctest`, including the previous branch's suites as the regression
    signal for "unchanged at `w = 0`".
17. `clang-format --dry-run --Werror` on changed files.
18. Walk `validation.md` and record every result, including what could not run.
