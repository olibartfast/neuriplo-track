# Feature Requirements — evaluation

> **Historical record.** Written before the work, delivered in PR TBD, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

Roadmap phase: [Phase 3 — Evaluation](../roadmap.md), all three bullets
("MOT-format output", "Metrics harness", "Reproducible comparison run").

## Goal

A tracker change can be shown to help or hurt with a number rather than an
opinion. Today it cannot: `specs/roadmap.md` states it outright — *"Comparing
trackers is the stated user need; right now there is no way to produce a
number."*

The concrete cost of that gap is on record. The mask-conditioned association cue
delivered in PR #4 was assessed by rendering two videos and eyeballing them at
two timestamps, and the conclusion — that at `--mask_iou_weight=0.5` the cue
essentially never changes an association decision — rests on nothing stronger
than that. It may well be correct. There is no way to tell, and no way to tell
whether a fix made it better.

After this, a run emits MOTChallenge-format results, a script turns those into
MOTA / IDF1 / IDsw, and one command replays a fixed set of detections through
every tracker and prints a comparison table.

## In Scope

- **MOT-format tracking output.** `--mot_output <file>` writes one line per
  tracked object per frame in MOTChallenge format:
  `frame,id,bb_left,bb_top,bb_width,bb_height,conf,-1,-1,-1`, 1-based frame
  numbering, written from `MultiObjectTrackingApp::processVideo` where tracks
  are already in hand.
- **Detection capture and replay.**
  - `--save_detections <file>` writes the detector's output in the same format
    with `id = -1` (MOT `det.txt` convention).
  - `--detections <file>` reads them back and runs the tracker over them with no
    model, no engine and no backend — the detector is not constructed at all.
  - Both use one shared reader/writer pair, since the two formats differ only in
    the id column.
- **Rendering becomes optional.** `--output` is currently mandatory:
  `processVideo` throws if the `cv::VideoWriter` will not open, so any run pays
  for H.264 encoding. It becomes optional, and when it is absent both the
  drawing calls and the writer are skipped.
- **Metrics harness.** `scripts/eval_mot.py`, built on the existing
  `trackers/BoTSORT/src/mot_metrics_evaluator.py` (py-motmetrics), taking a
  ground-truth file and a tracking output and printing MOTA, IDF1, IDP, IDR,
  precision, recall, MT/PT/ML, FP, FN, IDsw, FM, MOTP. Reachable from a
  documented command, which the current script is not.
- **Reproducible comparison run.** `scripts/compare_trackers.sh` runs every
  tracker over one captured detection file and emits a single table, one row per
  tracker. With ground truth it reports metrics; without it, it reports the
  descriptive counts that need no labels (tracks created, mean track length,
  max concurrent tracks).
- **Tests**: round-tripping a MOT file (write → read → identical values), 1-based
  frame numbering, the empty-frame case, and a malformed-line rejection.
- **Documentation**: `README.md` CLI table, `docs/` evaluation section,
  `AGENTS.md`, `specs/tech-stack.md`, `specs/roadmap.md`.

## Out of Scope

- **HOTA.** py-motmetrics does not compute it; HOTA requires
  [TrackEval](https://github.com/JonathonLuiten/TrackEval). Since TrackEval
  consumes exactly the MOT files produced here, it can be pointed at them
  without any further work in this repository, and that is the documented route.
  No HOTA number is computed or claimed by anything in this branch. The Phase 3
  roadmap bullet naming HOTA is amended rather than met.
- **Reproducing published benchmark numbers.** The bundled evaluator loads every
  ground-truth row and ignores MOT17's distractor classes and visibility column,
  so its MOTA will not match the MOTChallenge leaderboard even on MOT17 input.
  This harness is a **regression signal for changes made here**, not a claim of
  comparability. The caveats already written into the benchmark table in
  `docs/Tracking_Algorithms.md` continue to apply and are not weakened.
- **Shipping datasets or ground truth.** No sequence, no labels, no detector
  weights enter the repository. Scripts take paths.
- **Per-class evaluation.** `TrackedObject` carries no class id and the trackers
  pool all tracked classes into one association problem, so the class column is
  written as `-1` and evaluation is class-agnostic. `--classes` remains the way
  to restrict what is tracked. Adding a class id to `TrackedObject` is a change
  to a type whose stability policy is a Phase 4 item and is not made here.
- **A CI job that runs the comparison.** The plumbing is built so that one is
  possible later — replay needs no model and no GPU — but wiring it into
  `.github/workflows/ci.yml` waits until the harness has been used by hand
  enough to know what a meaningful fixture is.
- **Changing any tracker's behaviour.** Nothing in `trackers/` changes except
  the addition of nothing at all: this branch touches `app/`, `scripts/`,
  `tests/` and docs.
- **The mask-cue gating rework and BoT-SORT clear-match locking.** Both were the
  motivation for doing this first, and both are deliberately left until there is
  something to measure them with.

## Decisions

1. **MOT format is the interchange, not an internal type.** Every consumer worth
   having — py-motmetrics, TrackEval, the MOTChallenge tooling — already reads
   it. Emitting it means the metrics question becomes someone else's solved
   problem, and it is why HOTA can be declared out of scope without leaving the
   user stuck.
2. **Detections are captured to a file and replayed, rather than the detector
   being re-run per tracker.** The roadmap asks for a run over *"the same
   detections"*. Re-running the model per tracker gives the same detections only
   if the model is deterministic, and costs a full inference pass each time — the
   demo reel took roughly two hours per segmentation pass on this machine. A
   captured file makes the comparison exact by construction, fast enough to
   iterate on, and runnable with no model present, which is also what would let
   it become a CI job later.
3. **The detection file reuses the MOT format with `id = -1`.** That is the
   MOTChallenge `det.txt` convention, so one writer and one parser serve both
   paths and external `det.txt` files work as input for free.
4. **`--detections` skips detector construction entirely**, rather than building
   one and not calling it. A replay run must not require a backend, a model file
   or a GPU; if the detector were still constructed, it would.
5. **Metrics stay in Python.** py-motmetrics is mature and the existing script
   already wraps it. Reimplementing MOTA or IDF1 in C++ would add a maintenance
   burden and a second source of truth for numbers whose whole purpose is to be
   comparable with what everyone else reports.
6. **`scripts/eval_mot.py` supersedes `trackers/BoTSORT/src/mot_metrics_evaluator.py`
   rather than editing it in place.** That file sits inside vendored BoT-SORT
   source; changes there are re-litigated at every upstream sync. The new script
   is a small wrapper in `scripts/` and the vendored copy is left untouched.
7. **The comparison script reports label-free counts when no ground truth is
   given.** Most footage anyone has to hand is unlabelled. Tracks created, mean
   track length and max concurrent tracks are computable from the MOT output
   alone, are not accuracy metrics, and must be labelled as such — but they are
   what made the demo reel's "400 ids vs 25" observation, and computing them is
   strictly better than counting by eye.
8. **Frame numbering is 1-based.** MOTChallenge is 1-based and the bundled
   evaluator already compensates with `frame += 1`; `frameCount` in
   `processVideo` is 0-based at the point tracks are produced. Writing 1-based
   from the start means the wrapper needs no such adjustment.
9. **Output goes through a small writer type, not `std::ofstream` inline in the
   frame loop.** `processVideo` is already doing preprocessing, inference,
   postprocessing, mask assembly, tracking, drawing and encoding. The writer is
   testable on its own; an inline `<<` chain is not.

## Constraints and Context

- `MultiObjectTrackingApp::processVideo` opens the writer at line 111 and throws
  if it fails, before the loop starts — this is the mandatory-render behaviour
  that has to be relaxed.
- Tracks are available at the `tracker_->update(...)` call, with `frameCount`
  incremented at the end of the loop body.
- `TrackedObject` has `track_id`, `x`, `y`, `width`, `height`, `confidence` and
  no class id. `confidence` is populated by the OC-SORT and C-BIoU wrappers and
  left at zero by SORT, ByteTrack and BoTSORT, so the MOT `conf` column is not
  meaningful for every tracker and must be documented that way.
- `cv::VideoCapture` accepts a printf-style path (`img1/%06d.jpg`), which is how
  a MOTChallenge image sequence would be fed in; this is assumed, and validating
  it is a check in [validation.md](validation.md).
- Code must build clean under `-Wall -Wextra -Wpedantic -Werror` and match
  `.clang-format` (4-space, 120 columns).
- `-DBUILD_ONLY_LIB=ON` must still build: nothing added here may make `trackers`
  depend on `app/`.
- py-motmetrics is not a build dependency and must not become one. The C++ side
  never needs it; only the script does, and the script degrades to a clear error
  message when it is missing.
- Docs carrying hyperlinks are subject to the link-verification rule in
  `AGENTS.md`.
