# Roadmap

Part of the constitution; see [README.md](README.md) for how these
documents fit together.

Status as of 2026-08-18. Phase 0 records what already exists; later phases are
ordered by what unblocks the most downstream work, not by size.

## Phase 0 — Shipped

- [x] **Tracker abstraction** — `BaseTracker` interface with SORT, ByteTrack, and
      BoTSORT wrappers; tracker selection in `MultiObjectTrackingApp::createTracker`.
- [x] **Pluggable inference** — six backends selectable via `DEFAULT_BACKEND`
      (OpenCV DNN, ONNX Runtime, TensorRT, LibTorch, OpenVINO, LibTensorFlow).
- [x] **Detector boundary isolated** — detection flows through
      `neuriplo_tasks::TaskFactory` / `Detection`; no parallel detection types
      in this repo (`77820a3`).
- [x] **Standalone tracker library** — `BUILD_ONLY_LIB=ON` builds `trackers`
      without the CLI.
- [x] **Centralized dependency versions** — `versions.env` + `cmake/versions.cmake`
      + `cmake/DependencyValidation.cmake`.
- [x] **Sibling-checkout override** — local `../neuriplo` / `../neuriplo-tasks`
      trees are used instead of fetching, for cross-repo work.
- [x] **CI and lint** — Release/Ninja build plus a Debug `WERROR=ON` job on
      ubuntu-24.04; clang-format / clang-tidy / pre-commit wired up (`eae7176`).
- [x] **Docker deployment** — GPU and CPU images with compose files.
- [x] **Documentation set** — architecture, algorithms, build, MOT guide,
      code examples, end-to-end test, plus `AGENTS.md`.

## Phase 1 — Make the build trustworthy

The gap that blocks everything else: CI runs `ctest --test-dir build`, but the
project never calls `enable_testing()` or registers a test. The command
currently passes because there is nothing to run.

- [x] **Register a test suite.** `enable_testing()` plus a `tests/` target behind
      `NEURIPLO_TRACK_BUILD_TESTS`; three ctest suites (`trackers`,
      `track_config`, `utils`) covering `splitString` / `generateOutputPath` and
      the CLI → `TrackConfig` resolution
      ([2026-08-18-ocsort-cbiou](2026-08-18-ocsort-cbiou/requirements.md)).
      `CommandLineParser` argument validation is still uncovered: it calls
      `std::exit` on failure, so it needs a seam before it can be tested.
- [x] **Tracker unit tests against synthetic detections.** Every `BaseTracker`
      implementation is driven through scripted `Detection` frames — linear
      motion, occlusion gap, crossing objects, class filtering, degenerate
      input — with identity continuity asserted for OC-SORT and C-BIoU.
- [ ] **Pin the floating dependencies.** `NEURIPLO_VERSION="master"` and
      `BYTETRACK_VERSION="main"` mean two builds a week apart are not the same
      build — contradicting the reproducibility principle in
      [mission.md](mission.md). Pin both to tags or commit SHAs.
- [ ] **Move the ONNX Runtime version into `versions.env`.** `1.19.2` is
      hardcoded in `CMakeLists.txt` while every other version lives in one place.

## Phase 2 — Close the CLI gaps

- [ ] **Implement `--input_sizes` parsing.** The flag is documented in
      `README.md` and `AGENTS.md` and accepted by `CommandLineParser`, but the
      parse body is a stub that leaves `config.input_sizes` empty
      (`app/src/CommandLineParser.cpp`). Support `H,W` for fixed-channel models
      and `C,H,W` for fully dynamic ones, with validation and a clear error.
- [x] **Expose tracker tuning parameters.** Every `TrackConfig` field is now a
      CLI flag; unset flags fall back to per-algorithm defaults resolved in
      `makeTrackConfig` (`app/src/utils.cpp`)
      ([2026-08-18-ocsort-cbiou](2026-08-18-ocsort-cbiou/requirements.md)).
- [ ] **Fail fast on BoTSORT config paths.** Missing `--reid_onnx` or an
      unreadable INI should be a startup error naming the missing file, not a
      failure deep in the first frame.

## Phase 3 — Evaluation

Comparing trackers is the stated user need; right now there is no way to produce
a number.

- [ ] **MOT-format output.** Write per-frame tracking results in MOTChallenge
      format so external evaluators can consume a run.
- [ ] **Metrics harness.** Wire up MOTA / IDF1 / HOTA reporting over the MOT
      output — `trackers/BoTSORT/src/mot_metrics_evaluator.py` exists as a
      starting point but is not reachable from any project workflow.
- [ ] **Reproducible comparison run.** A script that runs all three trackers over
      the same detections on a fixed clip and emits one comparison table, so
      tracker changes have a regression signal.

## Phase 4 — Consumability

- [ ] **Install and export targets.** Neither `CMakeLists.txt` nor
      `trackers/CMakeLists.txt` has an `install()` or `export()` rule, so the
      "embed the tracker library elsewhere" use case requires vendoring the
      source tree. Add install rules and a package config so downstream projects
      can `find_package(neuriplo-track)`.
- [ ] **Version the tracker library.** Give `trackers` a `SOVERSION` and state a
      stability policy for `BaseTracker` / `TrackedObject` / `TrackConfig`.
- [ ] **Broaden backend CI coverage.** CI exercises only the default backend;
      the other five can break silently. Add at least an `OPENCV_DNN` job (no
      external download needed) and a configure-only check for the rest.

## Phase 5 — Capability

- [x] **OC-SORT and C-BIoU.** Implemented in-tree from the papers, wrapped behind
      `BaseTracker`, and selectable as `--tracker=OCSORT` / `--tracker=CBIoU`
      ([2026-08-18-ocsort-cbiou](2026-08-18-ocsort-cbiou/requirements.md)).
      Both are motion-only, so neither adds a dependency or a model asset.
- [ ] **Remaining trackers.** McByte extends BoT-SORT with a *temporally
      propagated* mask as an association cue: SAM seeds a tracklet's mask from
      its box, Cutie propagates it across frames. `neuriplo-tasks` already has
      instance segmentation (`InstanceSegmentationTask`), but that yields
      per-frame per-detection masks, not a per-tracklet propagated one — no
      SAM/Cutie-equivalent task exists upstream yet. Its non-mask half
      (clear-match locking around the assignment step) is implementable today.
      StrongSORT would reuse the BoTSORT Re-ID path; it has no row in the
      benchmark table in `docs/Tracking_Algorithms.md`, because the Roboflow
      suite does not implement it.
- [ ] **Let masks reach the tracker.** `MultiObjectTrackingApp::processVideo`
      keeps only `Detection` from the task result variant, so an
      `InstanceSegmentation` model's masks are discarded, and
      `BaseTracker::update` has no way to carry them. A prerequisite for any
      mask-conditioned association, McByte's or otherwise.
- [ ] **Batch and multi-stream processing.** `AppConfig::batch_size` exists and
      is parsed, but the pipeline processes one source frame-by-frame.
- [ ] **Performance instrumentation.** Per-stage timing (detect / track / draw)
      surfaced as a summary, so backend and tracker choices can be judged on
      latency and not just accuracy.

## Explicitly out of scope

Per [mission.md](mission.md): model training or export, new detector
implementations, novel tracking algorithms, and any server / VMS layer.
