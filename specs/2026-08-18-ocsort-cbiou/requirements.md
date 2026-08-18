# Feature Requirements — OC-SORT and C-BIoU trackers

> **Historical record.** Written before the work, delivered in PR #3, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

Roadmap phase: [Phase 5 — Capability](../roadmap.md), first bullet ("Additional
trackers"), plus the "Expose tracker tuning parameters" bullet from Phase 2 and
the "Register a test suite" bullet from Phase 1, both pulled forward (see
Decisions).

## Goal

A user can select `--tracker=OCSORT` or `--tracker=CBIoU` and get the same
end-to-end behaviour as the three existing trackers, and can tune any tracker's
parameters from the command line instead of recompiling. The two algorithms
close the motion-only gap identified against the
[Roboflow `trackers` benchmarks](https://trackers.roboflow.com/): OC-SORT
(61.9 MOT17 HOTA) and C-BIoU (63.0) sit between ByteTrack (60.1) and BoT-SORT
(63.7) without needing a Re-ID model.

## In Scope

- **OC-SORT** (`trackers/OCSORT/`), implemented in-tree from
  [arXiv 2203.14360](https://arxiv.org/abs/2203.14360), with the three
  contributions the paper names:
  - **ORU** — observation-centric re-update: on re-association after a gap, the
    Kalman filter is re-run over a virtual trajectory interpolated between the
    last real observation and the new one.
  - **OCM** — observation-centric momentum: the association cost adds a velocity
    direction-consistency term weighted by `inertia`, with direction estimated
    from observations `delta_t` frames apart.
  - **OCR** — observation-centric recovery: a second association round between
    still-unmatched detections and the *last observations* of unmatched tracks.
- **C-BIoU** (`trackers/CBIoU/`), implemented in-tree from
  [arXiv 2211.14317](https://arxiv.org/abs/2211.14317):
  - **Buffered IoU** — both detection and track boxes expanded by a scale factor
    before IoU is computed.
  - **Cascaded matching** — a first round at buffer `b1`, then a second round at
    the larger buffer `b2` over what is left.
  - **Non-Kalman motion model** — predicted state is the last observation plus
    the mean displacement over the previous `n` observations.
- **Wrappers** `OCSortWrapper` and `CBIoUWrapper` implementing `BaseTracker`,
  registered in `MultiObjectTrackingApp::createTracker` and
  `trackers/CMakeLists.txt`, following the five-step procedure in `AGENTS.md`.
- **Full tracker-parameter CLI exposure** (Phase 2 item): every field of
  `TrackConfig` settable from the command line — the seven existing ones
  (`max_age`, `min_hits`, `iou_threshold`, `track_buffer`, `track_thresh`,
  `high_thresh`, `match_thresh`) and the new per-algorithm ones.
- **A registered test suite** (Phase 1 item): `enable_testing()`, a `tests/`
  target, and `ctest` cases covering the new trackers against synthetic
  detection sequences plus the CLI→`TrackConfig` mapping.
- **Documentation**: `docs/Tracking_Algorithms.md`, `README.md`, `AGENTS.md`
  tracker list, `specs/tech-stack.md` (vendored-tracker table, CLI surface,
  build options), `specs/roadmap.md` status.

## Out of Scope

- **McByte and StrongSORT.** McByte's mask cue needs *temporal mask
  propagation*: SAM to seed a mask from a box when a tracklet is born, and
  Cutie to carry that mask forward frame to frame. `neuriplo-tasks` does supply
  instance segmentation (`InstanceSegmentationTask`: YOLO-seg, YOLOv10/26-seg,
  RF-DETR-seg, EdgeCrafter-seg → `InstanceSegmentation`), but per-frame instance
  masks are a different signal from a per-tracklet propagated mask, and no
  SAM/Cutie-equivalent task exists upstream. Two further gaps: the app extracts
  only `Detection` from the task result variant, so `InstanceSegmentation`
  results are dropped today, and `BaseTracker::update` takes `Detection` only.
  StrongSORT needs the Re-ID path. Both stay on the Phase 5 list.
- **Reproducing the published HOTA numbers.** No MOT17/DanceTrack data, no
  detector weights, and no metrics harness exist in this repo — that is Phase 3
  ("Evaluation"). The benchmark table stays a *reference*, never a claim about
  this implementation. Correctness evidence here is synthetic-sequence
  behaviour, not benchmark parity.
- **OC-SORT's optional `use_byte` low-score association.** Off in the original
  implementation's defaults; not implemented.
- **Changing the existing trackers' behaviour.** SORT, ByteTrack and BoTSORT
  keep their current defaults and code paths untouched.
- **The remaining Phase 1 and Phase 2 items**: pinning `NEURIPLO_VERSION` /
  `BYTETRACK_VERSION`, moving the ONNX Runtime version into `versions.env`,
  `--input_sizes` parsing, and BoTSORT config fail-fast.
- **Per-class tracking.** Like the existing trackers, detections of all tracked
  classes go into one association pool.

## Decisions

1. **In-tree implementation, not a vendored dependency.** No maintained
   standalone C++ OC-SORT or C-BIoU exists to fetch, and adding a floating
   `GIT_TAG` would contradict the reproducibility principle in
   [mission.md](../mission.md). Both algorithms live under `trackers/` next to
   SORT and BoTSORT.
2. **Reuse `HungarianAlgorithm` and `cv::KalmanFilter`.** Both are already in
   the `trackers` target (`trackers/SORT/Hungarian.hpp`, OpenCV). No new
   dependency is introduced by this feature.
3. **Per-tracker defaults resolved at config-build time.** `TrackConfig`'s
   `max_age` default is `1`, which would defeat OC-SORT's and C-BIoU's whole
   point (recovering a track across a gap). Rather than change a shared default
   and alter SORT's behaviour, `makeTrackConfig` applies algorithm-specific
   defaults (`max_age=30`, `min_hits=3`, `iou_threshold=0.3` for the two new
   trackers) and CLI flags override them. Existing trackers' effective defaults
   are unchanged.
4. **CLI overrides are optional-valued.** `AppConfig` holds `std::optional`
   overrides so "not passed" is distinguishable from "passed the default", which
   is what makes decision 3 expressible.
5. **`makeTrackConfig(const AppConfig&)` is a free function in `app/src/utils.cpp`.**
   The `AppConfig`→`TrackConfig` mapping currently happens inline in the
   `MultiObjectTrackingApp` constructor, which cannot be unit-tested without
   loading a model. Extracting it is the minimum change that makes decision 3
   testable.
6. **No test framework dependency.** `tests/` uses plain assertion helpers and
   `add_test`, matching the constitution's bias against unauthorised
   dependencies. The option is named `NEURIPLO_TRACK_BUILD_TESTS` because the
   root `CMakeLists.txt` already force-sets a cache variable named `BUILD_TESTS`
   to `OFF` for `neuriplo-tasks`.
7. **Tracker names accept both spellings.** `--tracker` takes `OCSORT` or
   `OC-SORT`, and `CBIoU` or `C-BIoU`, so the paper spelling works.
8. **`ocsort_det_thresh` keeps the paper default (0.6).** OC-SORT gates
   detections a second time; the app's `--min_confidence` default is 0.25, so
   this is documented in the CLI help and README rather than silently retuned.
9. **`TrackedObject::confidence` is populated by the new wrappers.** The field
   exists and is left at zero by the existing wrappers; the new ones fill it
   from the associated detection. No existing wrapper is changed.

10. **Association costs are non-negative and threshold-gated.** The bundled
    `HungarianAlgorithm` only warns on negative costs, so costs are expressed as
    distances. Two properties follow from how the solver works, both added after
    review: the offset that keeps costs non-negative must be the *same for every
    entry* (the solver picks a subset of columns when detections outnumber
    tracks, so a per-column offset scaled by detection score would penalise
    confident detections), and pairs already below the IoU threshold are given a
    prohibitive cost rather than being filtered only after assignment, so an
    invalid edge cannot displace a valid pairing.
11. **ORU rewinds to a snapshot of the last real observation.** Replaying the
    virtual trajectory on top of the drifted state both keeps the
    prediction-only error and integrates more frames than actually elapsed. Each
    real observation snapshots `statePost` / `errorCovPost`; re-association
    restores it, replays, and advances exactly `time_since_update` frames.
12. **C-BIoU observations carry their frame index.** The motion model divides
    displacement by frames spanned, not by the number of stored observations, so
    a gap cannot inflate the velocity estimate.
13. **Tuning values are validated by a pure function.** `validateTrackerOverrides`
    returns the first problem as a string and the parser reports and exits, so
    the rules are testable without spawning a process.

## Constraints and Context

- `BaseTracker::update(detections, frame)` is the only interface; the new
  trackers ignore `frame` like SORT and ByteTrack do (`static_cast<void>(frame)`).
- Detections arrive as `neuriplo_tasks::Detection` and must not be shadowed by a
  parallel type outside the tracker's own internal structs
  (mission.md principle 2, `AGENTS.md`).
- Class filtering against `TrackConfig::classes_to_track` happens in the
  wrapper, as in `SortWrapper` and `ByteTrackWrapper`.
- Code must build clean under `-Wall -Wextra -Wpedantic -Werror` (the CI Debug
  job) and match `.clang-format` (4-space, 120 columns).
- The `trackers` library must still build with `BUILD_ONLY_LIB=ON`, i.e. the new
  code must not depend on anything in `app/`.
- Docs carrying hyperlinks are subject to the link-verification rule in
  `AGENTS.md`.
