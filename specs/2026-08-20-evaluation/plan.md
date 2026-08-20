# Feature Plan — evaluation

> **Historical record.** Written before the work, delivered in PR TBD, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

## Group 1 — The MOT file

1. `app/inc/MotFile.hpp`, `app/src/MotFile.cpp`: a `MotRecord` value type
   (frame, id, x, y, w, h, conf), a `MotWriter` that appends records to an open
   stream, and `readMotFile(path)` returning `std::vector<MotRecord>` or an error
   describing the offending line. Frame numbers are 1-based on the way out and
   validated as `>= 1` on the way in.
2. `tests/test_mot_file.cpp`: write → read round trip preserves every field;
   1-based numbering; a frame with no records writes nothing; a truncated line, a
   non-numeric field and a zero frame number are each rejected with a message
   naming the line number.
3. Register both in `app/CMakeLists.txt` and `tests/CMakeLists.txt`.

*Observable:* `ctest` covers the format before anything writes one.

## Group 2 — Writing from a run

4. `AppConfig`: `motOutputPath`, `saveDetectionsPath`, `detectionsPath`, and
   `outputPath` becoming optional in effect (empty means no video).
5. `CommandLineParser`: `--mot_output`, `--save_detections`, `--detections`;
   `--output` no longer required. Reject `--detections` together with
   `--weights` or `--type`, since a replay run has no detector to configure.
6. `processVideo`: construct the writer and drawing calls only when
   `outputPath` is non-empty; write tracks to the MOT writer when
   `motOutputPath` is set; write detections when `saveDetectionsPath` is set.
7. `tests/test_track_config.cpp` (or a sibling): the new flags land in
   `AppConfig`, and the mutually-exclusive combinations are rejected by
   `validateTrackerOverrides`' sibling validator rather than at runtime.

*Observable:* a normal run with `--mot_output` produces a MOT file; a run
without `--output` produces no video and encodes nothing.

## Group 3 — Replay

8. A replay path that opens the source for frame count and size, reads the
   detection file, groups records by frame, and drives `tracker_->update` per
   frame without constructing a detector or an engine. Masks are not available
   on this path, so `--mask_iou_weight` is rejected with an explanatory error
   rather than silently ignored.
9. Make it work with no video source at all when the detection file is the only
   input and no rendering is requested — frame count comes from the file's
   maximum frame number.
10. `tests/`: a small hand-written detection file replayed through two trackers
    gives deterministic, differing track ids, and replaying the same file twice
    gives byte-identical MOT output.

*Observable:* `--detections det.txt --tracker=OCSORT --mot_output out.txt` runs
to completion on a machine with no model, no ONNX Runtime session and no GPU.

## Group 4 — Metrics

11. `scripts/eval_mot.py`: argparse over `--gt` and `--tracker-output`, calling
    py-motmetrics for MOTA / IDF1 / IDP / IDR / precision / recall / MT / PT /
    ML / FP / FN / IDsw / FM / MOTP, printing the rendered summary. A missing
    py-motmetrics exits with an install hint, not a traceback.
12. A header printed above every summary stating what the numbers are and are
    not: a regression signal for changes in this repository, not a MOTChallenge
    result, because distractor classes and visibility are not handled.
13. Leave `trackers/BoTSORT/src/mot_metrics_evaluator.py` untouched and note in
    the docs that it is vendored and superseded.

*Observable:* a MOT file and a ground-truth file produce a metrics table from one
documented command.

## Group 5 — The comparison run

14. `scripts/compare_trackers.sh`: takes a detection file, an optional ground
    truth, and an optional tracker list (default: all five); replays each,
    collects each MOT output, and prints one table.
15. Label-free columns computed from the MOT output alone — tracks created, mean
    track length, max concurrent tracks — always shown, and labelled as
    descriptive counts rather than accuracy.
16. Metric columns shown only when ground truth was supplied.

*Observable:* one command turns one detection file into one table comparing
every tracker, and it is the command the next tracker change gets judged by.

## Group 6 — Documentation and roadmap

17. `README.md`: the three new flags in the CLI table, and a short evaluation
    section showing capture → replay → compare.
18. `docs/`: an evaluation page covering the format, the harness, the TrackEval
    route for HOTA, and the honest limits from
    [requirements.md](requirements.md) Out of Scope.
19. `AGENTS.md`: the evaluation entry point, so the next change knows where the
    regression signal lives.
20. `specs/tech-stack.md`: the new CLI surface and the `scripts/` additions.
    `specs/roadmap.md`: Phase 3 checked, with the HOTA amendment recorded rather
    than quietly dropped.
21. Verify every relative link added in this branch resolves.

*Observable:* Phase 3 can be marked done without any of its bullets being
overstated.
