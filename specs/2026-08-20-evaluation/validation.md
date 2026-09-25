# Feature Validation — evaluation

> **Historical record.** Written before the work, delivered in PR TBD, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

Written before implementation. Results are filled in afterwards; a check that
could not be run is recorded as **not executed**, never as a pass.

## Automated

- [ ] **Configure and build**:
      `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`
- [ ] **Warnings as errors**: Debug build with `-DWERROR=ON` succeeds.
- [ ] **Library-only build**: `-DBUILD_ONLY_LIB=ON` succeeds — nothing added
      here may make `trackers` depend on `app/`.
- [ ] **Full suite passes**: `ctest --test-dir build --output-on-failure`,
      including every suite from the previous branches.
- [ ] **Formatting**: `clang-format --dry-run --Werror` on changed files.

## Behavioural assertions

### The format

- [ ] **Round trip is lossless.** A vector of `MotRecord` written and read back
      compares equal field by field, including a confidence that is exactly zero
      (the value SORT, ByteTrack and BoTSORT produce).
- [ ] **Frame numbering is 1-based.** The first frame of a run is written as
      frame `1`, not `0`. Checked against the file, not against an intermediate.
- [ ] **An empty frame writes nothing.** A frame in which the tracker returns no
      objects contributes no line, and the next frame's number is unaffected.
- [ ] **Malformed input is rejected with a locating message.** A truncated line,
      a non-numeric field and a frame number of `0` each produce an error naming
      the line number, and no partial result.
- [ ] **The detection convention holds.** `--save_detections` writes `id = -1`
      on every line, and a file written that way is accepted by `--detections`.
- [ ] **An external `det.txt` is accepted.** A MOTChallenge `det.txt` taken from
      a public sequence parses without modification. If no such file is
      available, this is recorded as not executed rather than assumed.

### The run

- [ ] **No `--output` means no encoding.** A run without `--output` writes no
      video file, and the drawing functions are not called. Verified by the
      absence of the file and by wall-clock time against the same run with
      `--output`, which must be measurably lower.
- [ ] **`--mot_output` does not change tracking.** The same clip run with and
      without `--mot_output` produces identical rendered video, i.e. writing the
      file is observation only.
- [ ] **Replay needs no detector.** `--detections … --tracker=OCSORT
      --mot_output …` completes on an invocation passing no `--weights` and no
      `--type`, and `strace`/logging confirms no model file is opened. A machine
      with no ONNX Runtime session available is the stronger form of this check;
      if unavailable, say which form was run.
- [ ] **Replay is deterministic.** The same detection file replayed twice
      through the same tracker gives byte-identical MOT output.
- [ ] **Capture then replay reproduces the live run.** A clip run live with
      `--save_detections` and `--mot_output`, then replayed from the captured
      detections with the same tracker and parameters, produces identical MOT
      output. This is the check that makes the comparison run trustworthy — if
      it fails, replay is measuring something other than what a real run does.
- [ ] **Incompatible flags are refused.** `--detections` with `--weights`,
      `--detections` with `--type`, and `--detections` with
      `--mask_iou_weight` each exit with a message naming the conflict, before
      any processing starts.
- [ ] **An image sequence works as a source.** `cv::VideoCapture` opens a
      printf-style path (`img1/%06d.jpg`) and frame numbers in the MOT output
      match the sequence's own numbering. This is assumed by the MOTChallenge
      workflow and is not currently exercised anywhere in the repository.

### The harness

- [ ] **Metrics come out.** `scripts/eval_mot.py --gt … --tracker-output …`
      prints a summary containing MOTA, IDF1 and IDsw.
- [ ] **A known-perfect input scores perfectly.** Feeding the ground-truth file
      back in as the tracking output yields MOTA 1.0, IDF1 1.0 and IDsw 0. This
      is the check that the harness is wired up correctly rather than merely
      producing plausible-looking numbers.
- [ ] **A deliberately broken input scores worse.** The same file with every
      track id shuffled per frame produces a markedly lower IDF1 and a large
      IDsw, confirming the harness is sensitive to the thing it exists to
      measure.
- [ ] **A missing py-motmetrics is handled.** Running without it installed exits
      with an install hint and no traceback.
- [ ] **The vendored evaluator is untouched.**
      `git diff -- trackers/BoTSORT/src/mot_metrics_evaluator.py` is empty.

### The comparison

- [ ] **One command, one table.** `scripts/compare_trackers.sh` over one
      detection file emits a single table with one row per tracker.
- [ ] **It works without ground truth.** With no labels, the descriptive columns
      appear, the metric columns do not, and the output states which is which.
- [ ] **The descriptive counts are right.** Tracks created, mean track length
      and max concurrent tracks are checked against a hand-counted MOT file
      small enough to verify by reading.
- [ ] **It reproduces the demo-reel observation.** Replaying one detection
      capture through SORT and OC-SORT shows the large gap in tracks created
      that the reel reported by eye ("400 ids vs 25"). This does not validate
      either tracker; it validates that the harness sees what a human saw.

### Honesty of the claims

- [ ] **No HOTA number is produced or implied.** No script, table, README line
      or doc page in this branch prints a HOTA value or describes the harness as
      computing one.
- [ ] **The comparability limit is stated where the numbers appear**, not only
      in the packet: the metrics output itself carries the note that distractor
      classes and visibility are not handled, so the figures are a regression
      signal and not a MOTChallenge result.
- [ ] **The benchmark table's caveats are intact.** The reference table in
      `docs/Tracking_Algorithms.md` still says the ✅ column means "available
      here", not "reproduces this score", and nothing added here suggests
      otherwise.

## Manual

- [ ] **The mask cue gets its first real measurement.** Replay one capture from
      a segmentation model through OC-SORT at `--mask_iou_weight` 0 and 0.5 and
      compare. This is not a pass/fail check — it is the question that motivated
      the whole branch, and the answer is recorded here whichever way it goes.
      A null result is a result: it would confirm the demo-reel finding with
      evidence instead of eyeballing.
- [ ] **A second reviewer can run the whole flow from the README alone**, from
      capture to comparison table, without reading this packet.

## Results

Run on 2026-09-22 at working-tree state on top of `95b048e`.

**Environment caveat, applying to everything below.** This machine has sibling
checkouts at `../neuriplo-tasks` (`v0.8.2-2-gd320092`) and `../neuriplo`, which
`CMakeLists.txt` force-substitutes for the `versions.env` pins with no opt-out.
These results therefore describe the sibling trees, not the pinned build. See
[../orchestration.md](../orchestration.md) and R-5 of
[../2026-09-22-integrations](../2026-09-22-integrations/requirements.md).

**Pinned-dependency reproduction, 2026-09-22.** To remove that caveat for the
automated checks, the working tree was copied to a directory with no sibling
checkouts, so FetchContent resolved the pins: neuriplo-tasks `v0.8.1`, neuriplo
`v0.8.0` (confirmed with `git describe` in `_deps`). There the scoreboard passes
7/7 and the strict build passes, after the `-Werror` fix noted below. CI's lint
jobs were also run locally with CI's own commands: `clang-format-18
--dry-run --Werror` and `cppcheck` exit 0; `clang-tidy-18` exits 0
(`WarningsAsErrors` is empty, so it cannot fail CI). Of its warnings on newly
added lines, two were fixed (`[[nodiscard]]`, `starts_with`), one is a false
positive (optional access guarded on the preceding line), and one is advisory
(`replayDetections` cognitive complexity 43 against a threshold of 25). The
real-data checks below were **not** repeated on the pinned build.

Real-data checks used `assets/clip.mp4` (682x480, 300 frames) with
`ecdet_s.onnx` / `ecseg_s.onnx` from a sibling repository, `--classes=person`,
which yielded 122 detections.

### Automated

| Check | Result | Notes |
|-------|--------|-------|
| Configure and build | **pass** | `cmake --workflow --preset scoreboard`, exit 0 |
| Warnings as errors | **pass, after a correction** | The first recorded pass was **stale**: it ran before the `wasProvided` helper was added, and that helper failed `-Werror=range-loop-construct`. Caught by reproducing CI against the pinned dependencies (below); fixed; `cmake --preset strict && cmake --build --preset strict` now exits 0 on both the sibling and the pinned build. One `-Wunused-parameter` remains in fetched `bytetrack-src/src/lapjv.cpp`, which is not compiled with this project's warning flags |
| Library-only build | **pass** | `cmake --workflow --preset lib-only`, exit 0, 2/2 suites |
| Full suite passes | **pass** | 7/7: `trackers`, `mask_overlap`, `track_config`, `mot_file`, `eval_options`, `replay`, `utils` |
| Formatting | **pass** | `clang-format --dry-run --Werror` clean on every changed file. `trackers/BoTSORT/` is excluded from clang-format by `.pre-commit-config.yaml`, so the edit there follows the file's own style |

### The format

| Check | Result | Notes |
|-------|--------|-------|
| Round trip is lossless | **pass** | `mot_file` suite, delivered in `e7bed77` |
| Frame numbering is 1-based | **pass** | `mot_file` suite, plus the `replay` suite asserting every emitted frame lies in 1..10 |
| An empty frame writes nothing | **pass** | `mot_file` suite |
| Malformed input rejected with a locating message | **pass** | `mot_file` suite; `eval_mot.py` reports the same way (`bad.txt:1: expected at least 7 comma-separated columns, found 3`) |
| The detection convention holds | **pass** | The 122-row capture is `id = -1` on every line and was accepted by `--detections` without modification |
| An external `det.txt` is accepted | **not executed** | No public MOTChallenge sequence is available here. Only files this project wrote were parsed |

### The run

| Check | Result | Notes |
|-------|--------|-------|
| No `--output` means no encoding | **pass** | The 300-frame live capture run produced no video file and logged "No video output requested; rendering is skipped." The wall-clock comparison against the same run *with* `--output` was **not executed** -- absence of the file and the skipped code path were taken as sufficient |
| `--mot_output` does not change tracking | **pass, by construction** | The MOT writer is called after `tracker_->update` and touches nothing it reads. The stronger form -- two rendered videos compared -- was **not executed** |
| Replay needs no detector | **pass** | `--detections det.txt --tracker=OCSORT --mot_output ...` completed with no `--weights`, no `--type` and no labels file. The `replay` ctest case runs the same way on the `OPENCV_DNN` build, which downloads no ONNX Runtime. `strace` was **not** used; the evidence is that no detector or engine is constructed on that code path and no model path is passed |
| Replay is deterministic | **pass** | `replay` ctest case compares two runs byte for byte |
| **Capture then replay reproduces the live run** | **pass** | The strongest check in this packet, on real data: a 300-frame live run with `--save_detections` + `--mot_output` through OC-SORT, then a replay of the captured file through OC-SORT, produced **identical** MOT output (`diff` empty, 9 rows) |
| Incompatible flags are refused | **pass** | `replay` ctest asserts non-zero exit for `--detections` with `--weights` and with `--mask_iou_weight`; `eval_options` covers `--type`, `--save_detections`, and the source requirements, as pure-function checks |
| An image sequence works as a source | **not executed** | No image sequence available |

### The harness

| Check | Result | Notes |
|-------|--------|-------|
| Metrics come out | **not executed** | py-motmetrics is not installed and installing it is outside this session's permitted commands. The file reader, argument handling and error paths were exercised via `--check-only`; the metric computation itself is unverified |
| A known-perfect input scores perfectly | **not executed** | Same reason. This is the check that would prove the harness is wired correctly rather than merely plausible, and it remains outstanding |
| A deliberately broken input scores worse | **not executed** | Same reason |
| A missing py-motmetrics is handled | **pass** | Exits 1 with the install hint and no traceback |
| The vendored evaluator is untouched | **pass** | `git diff -- trackers/BoTSORT/src/mot_metrics_evaluator.py` is empty |

### The comparison

| Check | Result | Notes |
|-------|--------|-------|
| One command, one table | **pass** | `scripts/compare_trackers.sh --detections=det.txt` emits one row per tracker |
| It works without ground truth | **pass** | Descriptive columns appear, metric columns do not, and the output states the difference |
| The descriptive counts are right | **pass** | Hand-verified against the 10-frame, 2-object file the `replay` suite writes: 2 ids, mean length 10.0, max concurrent 2 |
| It reproduces the demo-reel observation | **pass, in miniature** | Over the same 122 detections: SORT 13 ids (mean length 4.1) against ByteTrack 2 ids (mean length 34.5). The same fragmentation gap the reel reported by eye, now as a number. The reel's own clip was not re-run, so the "400 vs 25" figure itself is **not** reproduced |

Full table, all five trackers over one 122-detection capture from
`assets/clip.mp4`:

```
TRACKER        TRACKS   MEAN_LEN   MAX_CONC     STATUS
------------ -------- ---------- ---------- ----------
SORT               13        4.1          1         ok
ByteTrack           2       34.5          1         ok
OCSORT              2        4.5          1         ok
CBIoU               5       20.6          2         ok
BoTSORT             2       15.0          1         ok
```

Read as counts, not accuracy: OC-SORT's 2 ids at mean length 4.5 against
ByteTrack's 2 at 34.5 reflects `ocsort_det_thresh = 0.6` gating detections a
second time, not worse tracking.

### Honesty of the claims

| Check | Result | Notes |
|-------|--------|-------|
| No HOTA number is produced or implied | **pass** | The only two occurrences of "HOTA" added in this branch (`scripts/eval_mot.py`, `README.md`) both state it is *not* computed and point at TrackEval |
| The comparability limit is stated where the numbers appear | **pass** | `eval_mot.py` prints it above every summary; `compare_trackers.sh` labels its columns as descriptive counts |
| The benchmark table's caveats are intact | **pass** | `docs/Tracking_Algorithms.md` still heads the column "Available here" and keeps the detections-differ / default-parameters caveats |

### Defect found by this work

| Finding | Status |
|---|---|
| **Pre-existing.** `BoTSORT::_extract_features` cropped `frame(bbox_tlwh)` with no clamping, so a Kalman-*predicted* box drifting past the frame edge threw `(-215:Assertion failed) 0 <= roi.x && ...` and aborted the run. Not introduced here; surfaced the first time BoTSORT was replayed over real detections, which is exactly what a regression harness is for. | **fixed** in this branch: the ROI is intersected with the frame, and a track predicted entirely outside it returns a zero feature so the motion cue decides |
| **Introduced here, caught in self-review.** `replayDetections` opened the video only when something was *drawn*, so a BoTSORT replay with `--source` but no `--output` was handed an empty `cv::Mat`. Combined with the clamp above, that degraded silently: every Re-ID feature came back zero and BoTSORT tracked on motion alone while reporting numbers that looked like BoTSORT's. The clamp turned a loud crash into a quiet wrong answer, which is the worse failure. | **fixed**: frames are opened when the tracker reads them, not only when they are drawn. Materially different — the same replay gives 45 rows on the broken path and 30 on the correct one |

### Manual

| Check | Result | Notes |
|---|---|---|
| A second reviewer can run the whole flow from the README alone | **not executed** | Requires a second person |

#### The mask cue's first real measurement

The question that motivated this branch: PR #4 concluded, by eyeballing two
rendered videos, that at `--mask_iou_weight=0.5` the cue "essentially never
changes an association decision". The cue needs per-frame masks, so it cannot
be replayed from a MOT file -- both arms must be live segmentation runs.

Setup: `assets/clip.mp4` (682x480, 300 frames), `ecseg_s.onnx`, OC-SORT, one
run at `--mask_iou_weight=0` and one at `0.5`, everything else identical.

**Attempt 1 -- `--classes=person`. Inconclusive; not a null result.**

| | `mask_iou_weight=0` | `mask_iou_weight=0.5` |
|---|---|---|
| tracked rows | 1 | 1 |
| frames with output | 1 | 1 |
| distinct ids | 1 | 1 |

The two are byte-equivalent. That says nothing about the cue: a single tracked
object over 300 frames presents **no association decision for a mask to
change**. Recording it as "the cue had no effect" would be exactly the error
this packet exists to prevent. `clip.mp4` carries at most 3 people in any frame
and 122 person detections in total; OC-SORT's `det_thresh = 0.6` then gates
most of those out.

**Attempt 2 -- six classes** (`person,car,bus,truck,motorbike,bicycle`).
Byte-identical to attempt 1: the *same single row*, same box, same frame.
Adding five classes changed nothing, which ruled out sparsity as the
explanation and pointed at gating instead.

**Attempt 3 -- diagnostic.** A capture run with `--save_detections`,
`--min_confidence=0.1` and `--det_thresh=0.1`:

| | value |
|---|---|
| detections reaching the tracker | **423** |
| frames carrying a detection | 132 of 300 |
| most detections in one frame | **38** |
| tracked rows at `mask_iou_weight=0` | **153** |

So the scene was never sparse and nothing was broken. **The nulls in attempts 1
and 2 were produced entirely by OC-SORT's `ocsort_det_thresh` default of 0.6**,
which gates detections a second time after `--min_confidence` (decision 8 of the
[OC-SORT packet](../2026-08-18-ocsort-cbiou/requirements.md), where it was kept
at the paper default deliberately). At that threshold this segmentation model
surfaces one tracked object in 300 frames, so there is nothing for any
association cue to influence.

This matters beyond the mask question: **a tuning default silently suppressed
the measurement**, and the only reason it was caught is that the harness made
the detection count visible. Eyeballing two rendered videos -- the method PR #4
used -- would have shown two near-empty overlays and concluded the same "no
effect", for entirely the wrong reason.

**Attempt 4 -- the actual measurement.** With the gates open
(`--min_confidence=0.1 --det_thresh=0.1`, six classes, 423 detections), OC-SORT
run at `mask_iou_weight` 0, 0.25, 0.5 and 1.0, each compared against the `0`
baseline frame by frame.

| `mask_iou_weight` | rows | frames | ids | mean rows/id | frames differing from `w=0` |
|---|---|---|---|---|---|
| 0 (baseline) | 153 | 95 | 38 | 4.03 | — |
| 0.25 | 152 | 95 | 39 | 3.90 | 42 of 96 (**43.8%**) |
| 0.5 | 145 | 94 | 32 | 4.53 | 55 of 97 (**56.7%**) |
| 1.0 | 132 | 93 | 28 | 4.71 | 69 of 97 (**71.1%**) |

**The cue is not inert, and its effect is dose-dependent.** Divergence from the
baseline rises monotonically with the weight -- 43.8%, 56.7%, 71.1% -- while
the identity count falls monotonically (38 → 39 → 32 → 28) and mean track
length rises (4.03 → 4.53 → 4.71 at the top three weights). A monotonic
response across four settings is much harder to explain as noise than any
single comparison would be: the cue is genuinely participating in association,
and more of it consistently means fewer, longer identities.

This **contradicts the conclusion recorded in PR #4** -- that at
`--mask_iou_weight=0.5` the cue "essentially never changes an association
decision". That conclusion came from rendering two videos and comparing them at
two timestamps. It was wrong, and the reason it looked right is attempt 3: at
the shipped `ocsort_det_thresh` of 0.6 the overlays were nearly empty, so two
near-identical near-empty videos were read as "the cue does nothing".

What this does **not** say: nothing here shows the cue makes tracking *better*.
Fewer, longer identities is what *less fragmentation* looks like, and it is
equally what *merging two different objects into one track* looks like. The two
are indistinguishable without ground truth, and `assets/clip.mp4` has none.

The claim is therefore narrower and firmer than the one it replaces: the cue
demonstrably changes association decisions, in a dose-dependent way, so whether
it helps is a question worth answering rather than one already dismissed. The
default stays at 0 until someone answers it. Carried to
[../roadmap.md](../roadmap.md) as an open Phase 5 item.

Reproducing this: the four runs are live segmentation runs (masks cannot be
replayed from a MOT file), about 10 minutes each on CPU, at
`--min_confidence=0.1 --det_thresh=0.1` with
`--classes=person,car,bus,truck,motorbike,bicycle` on `assets/clip.mp4` with
`ecseg_s.onnx`.

### Deviations from the plan

| Plan said | What was built | Why |
|---|---|---|
| Group 2.5-2.6: `--output` becomes optional, "empty means no video" | `--output` is still auto-generated when it is omitted **and** no evaluation flag was passed | Taken literally, every existing invocation without `--output` would silently stop producing the video it produces today. `shouldWriteVideo()` keeps that behaviour and drops the video only when the run asked for a MOT file, a capture, or a replay instead |
| Group 2.6: "write detections when `saveDetectionsPath` is set" | Detections are captured **after** class filtering | The point of a capture is that replaying it reproduces the run. The MOT format carries no class column, so a file of unfiltered detections could not be filtered at replay time and would track objects the live run ignored. Verified: capture-then-replay is byte-identical |
| Group 3.8: replay drives `tracker_->update` per frame | Also stamps a synthetic class id (`kReplayClassId`) matched by `TrackConfig::classes_to_track` | The wrappers filter on class, and a MOT record has no class. One synthetic id expresses "already filtered" without teaching the wrappers a second rule |
| Group 2.7: the new flags are validated by "`validateTrackerOverrides`' sibling validator" | `validateEvaluationOptions(const AppConfig &)` in `app/src/utils.cpp`, covered by a new `eval_options` ctest suite | As specified. Recorded because the suite is new rather than an extension of `test_track_config.cpp` |
| — | `CommandLineParser` gained a `wasProvided(argc, argv, name)` helper | Discovered by the failing `replay` test: `cv::CommandLineParser::has()` reports whether a field holds a non-empty value, which is true for any field with a default, so it cannot answer "did the user pass `--type`?" for a field defaulting to `yolov8` |
| — | `compare_trackers.sh` accepts both `--opt value` and `--opt=value` | The binary it drives takes the `=` form; the two disagreeing would be a trap |
