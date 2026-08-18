# Feature Validation — mask-conditioned association

Written before implementation.

## Automated

- [x] **Configure and build**:
      `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`
- [x] **Warnings as errors**: Debug build with `-DWERROR=ON` succeeds.
- [x] **Library-only build**: `-DBUILD_ONLY_LIB=ON` succeeds (the mask utility
      must not depend on `app/`).
- [x] **Full suite passes**: `ctest --test-dir build --output-on-failure`,
      including the OC-SORT/C-BIoU suites from the previous branch.
- [x] **Formatting**: `clang-format --dry-run --Werror` on changed files.

### Behavioural assertions

- [x] **Mask IoU is correct.** Hand-computed cases: identical masks → 1.0;
      disjoint masks → 0.0; masks overlapping on exactly half their area →
      0.333…; an empty mask → 0.0; regions whose bounding boxes do not intersect
      → 0.0 without reading pixels.
- [x] **Cropping is lossless.** A full-frame mask cropped to its bounding box
      and compared against another cropped the same way gives the same IoU as
      the two full-frame masks would.
- [x] **The slicing default is transparent.** For every existing tracker,
      feeding `InstanceSegmentation` values produces exactly the track ids and
      boxes that feeding their sliced `Detection` values produces.
- [x] **`w = 0` changes nothing.** Every assertion in the previous branch's
      tracker suite still passes unchanged, and OC-SORT / C-BIoU produce
      identical output with masks supplied at weight 0 and with no masks at all.
- [x] **The cue actually decides.** In a scripted sequence where two objects'
      boxes overlap almost exactly but their masks are disjoint, identity is
      preserved at `w = 1` for OC-SORT and C-BIoU. The same sequence at `w = 0`
      is recorded — if it already succeeds without masks, the case is not proof
      and must be made harder until it fails without them.
- [x] **Graceful degradation.** A stream where only some detections carry masks
      tracks the mask-less ones by box IoU, with no crash and no dropped tracks.
- [x] **`makeTrackConfig`**: `mask_iou_weight` defaults to 0 for every tracker
      and is overridden by `--mask_iou_weight`.

## Manual

- [x] `--help` shows `--mask_iou_weight` with its default.
- [ ] End-to-end run with a YOLO-seg ONNX model at `w = 0` and `w = 0.5`.
      **Not executed** — no segmentation weights or clip in this environment;
      see Results.
- [x] Documentation links resolve.

## Diff review

- [x] No behaviour change to SORT, ByteTrack, or BoTSORT.
- [x] No new dependency; `versions.env` untouched.
- [x] No document describes this feature as McByte, and the "not propagated"
      limitation is stated wherever the cue is documented.
- [x] Nothing from the Out of Scope list appears in the diff.

## Definition of Done

- [x] Every In Scope item implemented or explicitly deferred here.
- [x] `specs/roadmap.md` reflects what shipped, and the McByte entry still
      records propagation as the outstanding blocker.
- [x] Spec, code, and docs tell the same story.

---

## Results

Run on 2026-08-18, Ubuntu, GCC, OpenCV 4.10.0.

### Executed and passing

| Check | Result |
|-------|--------|
| Release configure + build | exit 0 |
| Debug + `WERROR=ON` | exit 0; no warning from any file in this repository |
| `BUILD_ONLY_LIB=ON` | exit 0; 2/2 registered tests pass (the two app-dependent suites are skipped by design) |
| `ctest --test-dir build --output-on-failure` | 4/4 passed: `trackers`, `mask_overlap`, `track_config`, `utils` |
| `clang-format --dry-run --Werror` | clean after `clang-format -i` |
| `--help` | lists `--mask_iou_weight` with its meaning and default |

Every behavioural assertion above is implemented in `tests/test_mask_overlap.cpp`,
`tests/test_trackers.cpp` and `tests/test_track_config.cpp`, and passes.

### The discriminating case had to be made harder

`validation.md` required that the mask case fail without masks. It did not, at
first: a sequence where two objects swapped between positions 18px apart was
solved by OC-SORT at `w = 0`, because its momentum term absorbed the ambiguity.
Widening the swap to 30px makes box evidence point at the wrong pairing by
0.67 vs 0.33 IoU — more than the momentum term can correct — and the case now
behaves as specified:

- `w = 1`: track 1 follows the tall object across both positions; identity held.
- `w = 0`: track 1 sticks to a position, so its reported height alternates
  60/40; identity broken.

Both assertions are in `testMaskCueDecides`, including the negative one, with a
comment stating that if box-only association ever solves this case, the sequence
must be made harder rather than the assertion relaxed.

### Defects found during implementation

1. **`libneuriplo-tasks.a` was not position independent.** `MaskOverlap.cpp` is
   the first code here to reference `ImageMatrix`, which pulls
   `image_matrix.cpp.o` into `libtrackers.so` and failed the link with
   `recompile with -fPIC`. Fixed with `CMAKE_POSITION_INDEPENDENT_CODE ON`
   (requirements.md decision 7). A latent defect this feature exposed rather
   than caused.
2. **OC-SORT never received the mask weight.** The wrapper's constructor call
   had been reflowed by clang-format, so the edit adding the argument silently
   did not apply, and OC-SORT ran with the default weight of 0 while appearing
   to work. Caught because the `w = 1` and `w = 0` runs produced byte-identical
   output — the test that proves the cue *changes* something is what exposed it.
3. **`update({})` became ambiguous** once `update` was overloaded on the
   detection type. Only the test suite called it that way; recorded in
   requirements.md decision 1 as a cost of the overload.

### Not executed

- **End-to-end run with a real segmentation model.** No YOLO-seg weights and no
  clip are available here, so the path from `InstanceSegmentationTask` through
  `TaskFactory` to the tracker has not been exercised against a live model. What
  is covered: the mask geometry the postprocessors produce is reproduced exactly
  in the tests (full-frame 0/255 `ImageMatrix`, `mask_height`/`mask_width` = frame
  size), the variant extraction is a three-line change, and both tracker paths
  are tested. What remains unverified is that a real model's masks are as clean
  as the synthetic ones — which is a tuning question for `--mask_iou_weight`, not
  a correctness one.

### Post-review changes (2026-08-18)

Qodo raised three findings on the PR. All were checked against the code.

| Finding | Verdict | Evidence |
|---------|---------|----------|
| Stale masks are reused after a missed frame | **Confirmed, fixed** | Reproduced before changing anything: a segmented object moving 8px/frame across a 5-frame gap kept its identity at `w = 0` and **lost** it at `w = 1`, for both trackers. The cue was actively breaking occlusion recovery. Masks are now dropped on a missed frame; `testMaskDoesNotBlockRecovery` fails without that change. |
| Mixed `Detection` / `InstanceSegmentation` results are dropped | **Correct in principle, unreachable today** | Every task emits one result type — `InstanceSegmentationTask` only ever pushes `InstanceSegmentation`, so no frame can currently mix them. Hardened anyway: plain detections are carried as mask-less entries. |
| Overload ambiguity for empty initializer lists (raised as an alternative design) | **Acknowledged, kept** | The unified-observation-type alternative would change every wrapper and caller; the overload plus slicing default keeps existing trackers untouched. The `update({})` cost is recorded in requirements.md decision 1. |

This gap was mine to have caught: the occlusion tests all ran at `w = 0` and the
mask tests contained no gaps, so nothing in the suite crossed "cue on" with
"detection missing". That intersection is now covered.

### Deviations from the specification

None. The two additions to the spec during implementation (the PIC decision and
the `update({})` ambiguity) are recorded in requirements.md rather than left
undocumented.
