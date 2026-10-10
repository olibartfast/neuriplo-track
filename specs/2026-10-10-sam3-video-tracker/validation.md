# Feature Validation — SAM 3.1 as a video tracker

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
- [ ] **Library-only build**: `-DBUILD_ONLY_LIB=ON` succeeds, and `ldd
      libtrackers.so` shows no new inference library compared with `master`.
- [ ] **Full suite passes**: `ctest --test-dir build --output-on-failure`,
      including every suite from previous branches.
- [ ] **Formatting**: `clang-format --dry-run --Werror` on changed files.

## Behavioural assertions

All driven by `ScriptedPropagator`; none needs weights or video.

### Births and identity

- [ ] **A detection births exactly one object.** An unmatched detection
      triggers one `addObject` with its box; the next frames, with SAM
      reporting the object present and the detector agreeing, produce the same
      id and no further `addObject`.
- [ ] **Propagation holds identity where boxes do not.** Two objects whose
      detection boxes swap positions (the 30px swap from
      `testMaskCueDecides`) keep their ids because the propagator reports each
      id at its true position. The same detection sequence through OC-SORT at
      `w = 0` is recorded; if OC-SORT also holds identity, the case is not
      proof and must be made harder until it does not.
- [ ] **`min_hits` gates output.** An object is not reported until it has
      `min_hits` detection matches.

### Occlusion and loss

- [ ] **Absent resumes.** Presence below `sam_presence_thresh` for
      `k < max_age` frames: no output during the gap, the same id afterwards,
      no `removeObject`, no new `addObject`.
- [ ] **Absent too long is lost.** The same with `k > max_age`: exactly one
      `removeObject`, and the re-appearing detection births a new id.
- [ ] **Unsupported masklets die.** SAM reports an object present for
      `max_age + 1` frames with no detection near it: it is removed and stops
      being reported. The negative case — a detection-free gap shorter than
      `max_age` — keeps it.

### Drift and capacity

- [ ] **Drift re-prompts the same id.** A matched pair with box IoU below
      `sam_reprompt_iou` issues one `reprompt(id, detectionBox)`; above it, none.
- [ ] **Capacity is respected.** With `sam_max_objects = 2` and three
      simultaneous new detections, two `addObject` calls happen, the third
      detection is untracked, the warning is logged once across many frames,
      and nothing crashes.

### Interface

- [ ] **Both overloads.** The `Detection` overload tracks; the
      `InstanceSegmentation` overload produces identical output at
      `mask_iou_weight = 0`.
- [ ] **Class filter.** Detections of untracked classes never reach
      `addObject`.
- [ ] **Empty frame is an error.** `update` with an empty `cv::Mat` throws
      `std::invalid_argument`.
- [ ] **No detections, no objects.** An empty detection vector on every frame
      produces no `addObject` and no output.

### Configuration

- [ ] `canonicalTrackerName` maps `SAM3`, `sam3`, `SAM3.1`, `SAM-3.1`, `SAM31`
      to `SAM3`, and every existing spelling still resolves as before.
- [ ] `makeTrackConfig`: `SAM3` defaults (`max_age = 30`, `min_hits = 3`,
      `iou_threshold = 0.3`, the four `sam_*` values) and each override.
- [ ] Before Group 4: `--tracker=SAM3` fails at startup with a message saying
      the propagator is unavailable, not with a null-tracker error.

## Manual

- [ ] `--help` shows every `sam_*` flag with its default.
- [ ] Upstream issue (Group 0) opened and linked here.
- [ ] End-to-end run with a SAM 3.1 export on a clip with a crossing and an
      occlusion, `--tracker=SAM3` vs `--tracker=OCSORT`, compared with
      `scripts/compare_trackers.sh` from the same captured detections.
      **Expected to be not executed** until the upstream task exists.
- [ ] Missing `--sam_model` file is a startup error naming the path.
- [ ] Documentation links resolve (`AGENTS.md` hyperlink rule).

## Diff review

- [ ] No SORT / ByteTrack / BoTSORT / OC-SORT / C-BIoU behaviour change.
- [ ] No inference library, ONNX session, or Python call under `trackers/`.
- [ ] No weights, ONNX files, or export scripts committed.
- [ ] The SAM License and gated access are stated wherever the tracker is
      documented.
- [ ] No document calls this McByte.
- [ ] Nothing from the Out of Scope list appears in the diff.

## Definition of Done

- [ ] Every In Scope item implemented or explicitly deferred here — Group 4 may
      be deferred to a follow-up only if Group 0 has not landed, and the
      roadmap must say so.
- [ ] `specs/roadmap.md` reflects what shipped.
- [ ] Spec, code, and docs tell the same story.

---

## Results

*To be filled in after implementation.*
