# Feature Validation — OC-SORT and C-BIoU trackers

Written before implementation. Each box is checked only after the command was
actually run, with deviations recorded verbatim in the Results section.

## Automated

- [x] **Configure** succeeds:
      `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDEFAULT_BACKEND=ONNX_RUNTIME`
- [x] **Build** succeeds with warnings as errors:
      `cmake -S . -B build-werror -DCMAKE_BUILD_TYPE=Debug -DWERROR=ON && cmake --build build-werror -j$(nproc)`
- [x] **Library-only build** still works (no `app/` coupling):
      `cmake -S . -B build-lib -DBUILD_ONLY_LIB=ON && cmake --build build-lib -j$(nproc)`
- [x] **Test suite runs and is non-empty**:
      `ctest --test-dir build --output-on-failure` reports ≥ 3 tests, all passing.
- [x] **Formatting** clean on changed files:
      `clang-format --dry-run --Werror <changed .cpp/.hpp>`

### Behavioural assertions (in `tests/`)

Every case feeds scripted `neuriplo_tasks::Detection` frames — no model, no
video, no weights.

- [x] **Steady linear motion** — a box translating at constant velocity for 20
      frames yields exactly one track id, stable from the frame `min_hits` is
      satisfied onwards. Asserted for SORT, ByteTrack, OC-SORT and C-BIoU.
- [x] **Occlusion gap** — the same object, with detections suppressed for 5
      consecutive frames, keeps its original track id after reappearing.
      Asserted for OC-SORT (ORU/OCR) and C-BIoU (buffered matching); SORT and
      ByteTrack are exercised but not asserted on, since their configured
      `max_age` does not promise recovery.
- [x] **Crossing objects** — two boxes moving toward each other, meeting and
      separating, end with the same two ids they started with (no swap), for
      OC-SORT and C-BIoU.
- [x] **Class filtering** — detections whose `class_id` is absent from
      `classes_to_track` never produce a track, for both new trackers.
- [x] **Empty and degenerate input** — a frame with no detections, and a frame
      of zero-area boxes, return without crashing and without inventing tracks.
- [x] **Confidence propagation** — `TrackedObject::confidence` is non-zero and
      equal to the associated detection's confidence for both new trackers.
- [x] **`makeTrackConfig` defaults** — `OCSORT` and `CBIoU` resolve to
      `max_age=30`, `min_hits=3`, `iou_threshold=0.3`; `SORT`, `ByteTrack`, and
      `BoTSORT` resolve to exactly the `TrackConfig` defaults they get today.
- [x] **`makeTrackConfig` overrides** — an explicit CLI value wins over the
      algorithm default for every exposed parameter.
- [x] **Alias spellings** — `OC-SORT` and `C-BIoU` resolve to the same trackers
      as `OCSORT` and `CBIoU`.

## Manual

- [x] `./build/neuriplo-track --help` lists `OCSORT` and `CBIoU` under
      `--tracker` and shows every tracker-tuning flag with its default.
- [ ] An unknown `--tracker=` value still fails with the existing clear error.
      **Not executed** — unreachable without a loadable model; see Results.
- [ ] End-to-end run on a real clip with a YOLO ONNX model for
      `--tracker=OCSORT` and `--tracker=CBIoU`: boxes are drawn, ids persist
      across frames, no crash, output video written.
      **Not executed** — no model weights or video in this environment; see
      Results and `docs/End_To_End_Test.md`.
- [x] Documentation links resolve (`AGENTS.md` hyperlink rule): relative paths
      exist, new arXiv/benchmark URLs return 200.

## Diff review

- [x] No change to `SortWrapper`, `ByteTrackWrapper`, `BoTSORTWrapper`, or their
      algorithms — the "no behaviour change to existing trackers" boundary in
      requirements.md holds.
- [x] No new fetched dependency, no new `find_package`, `versions.env` untouched.
- [x] Nothing from the Out of Scope list (McByte, StrongSORT, `--input_sizes`,
      dependency pinning, BoTSORT fail-fast) appears in the diff.

## Definition of Done

- [x] Every In Scope item is implemented, or explicitly deferred here with a
      reason.
- [x] Benchmark numbers in the docs are attributed to the Roboflow reference
      implementations and are **not** presented as measurements of this port.
- [x] `specs/roadmap.md` reflects what actually shipped.
- [x] Spec, code, and docs tell the same story in one branch.

---

## Results

Run on 2026-08-18, Ubuntu, GCC, OpenCV 4.10.0, ONNX Runtime 1.19.2 backend.

### Executed and passing

| Check | Command | Result |
|-------|---------|--------|
| Configure | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDEFAULT_BACKEND=ONNX_RUNTIME` | exit 0 |
| Warnings-as-errors build | `cmake -S . -B build-werror -DCMAKE_BUILD_TYPE=Debug -DWERROR=ON && cmake --build build-werror` | exit 0; the only warning emitted comes from the fetched `bytetrack-src/src/lapjv.cpp`, which is not compiled with this project's warning flags |
| Library-only build | `cmake -S . -B build-lib -DBUILD_ONLY_LIB=ON && cmake --build build-lib` | exit 0; `libtrackers.so` built, no `app/` target |
| Test suite | `ctest --test-dir build --output-on-failure` | 3/3 passed (`trackers`, `track_config`, `utils`); 1/1 in the library-only build, where the two app-dependent suites are skipped by design |
| Formatting | `clang-format --dry-run --Werror <changed files>` | clean (violations found on first run were fixed by `clang-format -i`, then re-verified) |
| Help output | `./build/app/neuriplo-track --help` | lists `OCSORT` and `CBIoU` under `--tracker` and all thirteen tuning flags with their defaults |
| Link check | `curl -sI -L` on the new URLs; `ls` on relative targets | arXiv 2203.14360, arXiv 2211.14317, roboflow/trackers#513, trackers.roboflow.com all 200; relative targets exist |

All behavioural assertions listed above are implemented in
`tests/test_trackers.cpp` and `tests/test_track_config.cpp` and pass.

### Negative control

The occlusion-gap assertion was verified to be non-vacuous: with the gap
temporarily extended to 45 frames (beyond the 30-frame `max_age`), the test
fails for both OC-SORT and C-BIoU with
`!ids_after_gap.empty() && *ids_after_gap.begin() == id_before_gap`, and passes
again at the specified 5-frame gap.

### Defect found and fixed during validation

The same negative control surfaced a real bug: `HungarianAlgorithm::Solve`
prints `All matrix elements have to be non-negative.` to stderr for every
negative cost entry, and OC-SORT's first implementation used the reward form
`-(IoU + inertia * direction * score)`. The solver's row reduction made the
results correct anyway, which is why the tests passed while stderr filled up at
frame rate. Both OC-SORT cost matrices were rewritten in non-negative distance
form (see `trackers/OCSORT/OCSort.cpp`); the assignment is unchanged because the
offset is constant per column.

### Not executed

- **End-to-end run on a real clip.** No model weights and no video are available
  in this environment, so `--tracker=OCSORT` / `--tracker=CBIoU` have not been
  run against a live detector. The tracker path itself is covered by the
  synthetic suite; what remains unverified is the wiring from a real
  `neuriplo_tasks::Detection` stream and the drawing/writing path, which is
  shared with the existing trackers and unchanged.
- **Unknown `--tracker` value error.** The app fails at inference-engine setup
  before reaching `createTracker` unless a loadable model is supplied, so the
  error message could not be observed end to end. The logic behind it is covered:
  `canonicalTrackerName("DeepSORT")` returning empty is asserted in
  `tests/test_track_config.cpp`, and `createTracker`'s `nullptr` fallback is
  unchanged.

### Deviations from the specification

None. Scope, decisions, and out-of-scope boundaries in
[requirements.md](requirements.md) were followed as written; the diff touches no
existing tracker implementation or wrapper, adds no dependency, and leaves
`versions.env` untouched.
