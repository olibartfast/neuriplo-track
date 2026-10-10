# Feature Plan — SAM 3.1 as a video tracker

> **Historical record.** Written before the work, delivered in PR TBD, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

Groups 1–3 and 5–6 can merge without Group 0. Group 4 waits for it.

## Group 0 — Upstream contract (neuriplo-tasks, tracked, not built here)

0. Open an issue in neuriplo-tasks for a stateful promptable video
   segmentation task running the SAM 3.1 tracker graphs through neuriplo. The
   contract this repository needs from it:
   - `reset()`; `addObject(id, box, frame)`; `reprompt(id, box, frame)`;
     `removeObject(id)`; `propagate(frame) → [{id, mask, presence}]`;
     `capacity()`;
   - masks as full-frame 0/255 `ImageMatrix`, matching
     `InstanceSegmentation::mask`, so `MaskRegion` cropping applies unchanged;
   - presence as a probability in `[0, 1]`, not a logit;
   - the memory bank and multiplex buckets owned by the task, not the caller;
   - export provenance (exporter repo and commit) pinned in its own
     `versions.env`.

*Observable:* an issue link recorded in validation.md.

## Group 1 — The seam

1. `trackers/common/MaskPropagator.hpp`: `PropagatedMask { int id; MaskRegion
   mask; float presence; }` and the abstract `MaskPropagator` with the Group 0
   operations, expressed in OpenCV / `MaskRegion` types only.
2. `tests/ScriptedPropagator.hpp`: a test double driven by a per-frame script
   of `{id, box, presence}`, which records every `addObject`, `reprompt` and
   `removeObject` call for assertion.
3. Register in `trackers/CMakeLists.txt` and `tests/CMakeLists.txt`.

*Observable:* the interface compiles under `BUILD_ONLY_LIB=ON` with no new link
dependency.

## Group 2 — The tracker

4. `trackers/SAM3/Sam3Tracker.{hpp,cpp}`: the seven-step loop from
   requirements.md, with Hungarian assignment reusing `HungarianAlgorithm` and
   the box/mask blend reusing `trackers/common/MaskOverlap`.
5. Per-object state: id, frames since last detection match, frames absent,
   hit count, last detection confidence.
6. Capacity handling with a once-per-run warning.

*Observable:* `Sam3Tracker` driven directly by `ScriptedPropagator` in tests.

## Group 3 — Wrapper, configuration, CLI

7. `include/Sam3Wrapper.hpp`, `trackers/Sam3Wrapper.cpp`: constructor takes
   `TrackConfig` and `std::unique_ptr<MaskPropagator>`; both `update`
   overloads; class filter applied before association, as other wrappers do;
   empty frame → `std::invalid_argument`.
8. `TrackConfig`: `sam_presence_thresh` (0.5), `sam_det_thresh` (0.5),
   `sam_reprompt_iou` (0.5), `sam_max_objects` (64), `sam_model_path` ("").
9. `canonicalTrackerName`: strip `.` as a separator; map `sam3`, `sam31` →
   `SAM3`.
10. `makeTrackConfig`: `SAM3` gets `max_age = 30`, `min_hits = 3`,
    `iou_threshold = 0.3`, then CLI overrides as for every tracker.
11. `CommandLineParser` / `AppConfig::TrackerOverrides`: `--sam_presence_thresh`,
    `--sam_det_thresh`, `--sam_reprompt_iou`, `--sam_max_objects`,
    `--sam_model`.

*Observable:* `--help` lists the flags; `track_config` suite covers them.
`--tracker=SAM3` is **not** yet runnable: until Group 4, `createTracker` has an
explicit `SAM3` branch that throws "SAM 3 propagator unavailable in this build"
rather than falling through to the generic null-tracker error.

## Group 4 — App wiring (after Group 0 lands)

12. `app/src/Sam3TaskPropagator.{hpp,cpp}`: adapts the upstream task to
    `MaskPropagator`. Lives in `app/`, not `trackers/`, so the library never
    links an inference engine.
13. `MultiObjectTrackingApp::createTracker`: `SAM3` builds the adapter from
    `sam_model_path` and injects it. A missing or unreadable model path is a
    startup error naming the file.
14. Bump `NEURIPLO_TASKS_VERSION` in `versions.env` to the release carrying the
    task.

*Observable:* `--tracker=SAM3 --sam_model=<path>` runs end to end on a clip.

## Group 5 — Documentation

15. `docs/Tracking_Algorithms.md`: a SAM 3.1 section — propagation vs. motion
    model, detector-prompted loop, presence vs. lost, drift re-prompting,
    capacity, the frame requirement, and how it differs from both the per-frame
    mask cue and McByte.
16. `README.md`: flag table, an example, licence and gated-weights note.
17. `AGENTS.md`: the seam and the "frame required" list;
    `specs/tech-stack.md`; `specs/roadmap.md`.

## Group 6 — Verification

18. Release, Debug + `WERROR=ON`, and `BUILD_ONLY_LIB=ON` builds.
19. Full `ctest`; every existing suite is the regression signal for "nothing
    else changed".
20. `clang-format --dry-run --Werror` on changed files.
21. Walk validation.md and record every result, including what could not run.
