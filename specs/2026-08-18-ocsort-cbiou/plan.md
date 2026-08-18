# Feature Plan — OC-SORT and C-BIoU trackers

> **Historical record.** Written before the work, delivered in PR #3, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

Task groups run in dependency order; each ends in something observable. Commit
per group.

## Group 1 — Config surface

1. Extend `TrackConfig` with the per-algorithm parameters: `ocsort_delta_t`,
   `ocsort_inertia`, `ocsort_det_thresh`, `cbiou_b1`, `cbiou_b2`,
   `cbiou_motion_n`. Existing fields and the existing constructor keep their
   meaning and defaults.
2. Add `std::optional` tracker-parameter overrides to `AppConfig` and parse the
   new flags in `CommandLineParser` (only set an override when
   `parser.has(...)`). Extend `--tracker`'s help text and the usage examples.
3. Add `makeTrackConfig(const AppConfig&)` to `app/src/utils.cpp` /
   `app/inc/utils.hpp`: build a `TrackConfig`, apply the algorithm-specific
   defaults, then apply the CLI overrides. Call it from
   `MultiObjectTrackingApp`'s constructor in place of the inline construction.

*Observable:* `./build/neuriplo-track --help` lists every tracker parameter; the
existing trackers still run unchanged.

## Group 2 — OC-SORT

4. `trackers/OCSORT/OCSortKalmanTracker.{hpp,cpp}`: SORT-style 7-state
   `cv::KalmanFilter` box tracker extended with an observation history keyed by
   age, `last_observation`, the `delta_t` velocity direction, and the ORU
   virtual-trajectory re-update on re-association.
5. `trackers/OCSORT/OCSort.{hpp,cpp}`: the tracker loop — `det_thresh` gating,
   predict, OCM-weighted first association (Hungarian over
   `-(IoU + inertia · direction consistency · score)`), OCR second association
   against last observations, track birth/death, output rule.
6. `include/OCSortWrapper.hpp` + `trackers/OCSortWrapper.cpp`: class filtering,
   `Detection` → internal box, `TrackedObject` out with `confidence` set.
7. Register the sources in `trackers/CMakeLists.txt` (sources, warning flags,
   include dir) and the `OCSORT` / `OC-SORT` branch in
   `MultiObjectTrackingApp::createTracker`.

*Observable:* `--tracker=OCSORT` runs end to end and draws stable IDs.

## Group 3 — C-BIoU

8. `trackers/CBIoU/CBIoUTracker.{hpp,cpp}`: buffered-IoU helper, the
   mean-displacement motion model over the last `n` observations, and the
   two-round cascaded association at `b1` then `b2`.
9. `include/CBIoUWrapper.hpp` + `trackers/CBIoUWrapper.cpp`, mirroring the
   OC-SORT wrapper.
10. Register sources, include dir, and the `CBIoU` / `C-BIoU` branch in
    `createTracker`.

*Observable:* `--tracker=CBIoU` runs end to end.

## Group 4 — Test suite

11. `tests/CMakeLists.txt` behind `NEURIPLO_TRACK_BUILD_TESTS` (default `ON`),
    plus `enable_testing()` and `add_subdirectory(tests)` in the root
    `CMakeLists.txt`; a tiny header-only assertion helper (`tests/test_util.hpp`).
12. `tests/test_trackers.cpp`: synthetic `neuriplo_tasks::Detection` sequences
    driven through every `BaseTracker` implementation that needs no model —
    steady linear motion (one stable ID), an occlusion gap (ID recovered), two
    objects crossing (no ID swap), class filtering, and an empty-detection frame.
13. `tests/test_track_config.cpp`: `makeTrackConfig` defaults per algorithm and
    CLI override precedence.
14. `tests/test_utils.cpp`: `splitString` and `generateOutputPath`, the
    zero-dependency Phase 1 seams.

*Observable:* `ctest --test-dir build --output-on-failure` reports real,
non-empty results for the first time.

## Group 5 — Documentation and spec alignment

15. `docs/Tracking_Algorithms.md`: sections for OC-SORT and C-BIoU, updated
    comparison tables and "available here" column, configuration guidance for
    the new parameters.
16. `README.md` (feature list, tracker list, CLI options table, examples),
    `AGENTS.md` (architecture, tracker list, test instructions).
17. `specs/tech-stack.md` (vendored trackers, CLI surface, build options,
    tooling), `specs/roadmap.md` (tick the delivered items, note what remains).

## Group 6 — Verification

18. Configure and build with `-DWERROR=ON` and run the full `ctest` suite.
19. Run `clang-format --dry-run` over the added and changed files.
20. Walk through `validation.md` and record every result, including anything
    that could not be executed in this environment.
