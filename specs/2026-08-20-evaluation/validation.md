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

Filled in after implementation.

| Check | Result | Notes |
|-------|--------|-------|
| | | |
