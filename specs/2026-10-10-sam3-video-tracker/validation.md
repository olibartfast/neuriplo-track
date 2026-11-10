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

Recorded 2026-11-10, branch `feat/sam3-video-tracker` on `origin/master`
`17ceca5`, sibling checkouts `neuriplo@0.11.4-dev` + `neuriplo-tasks@0.8.6-dev`
(no opt-out exists, so the local build used the siblings, not the pins; CI is
the authority on the pinned `v0.11.3` / `v0.8.5` build).

### Automated

- [x] **Configure and build**: `cmake --build --preset scoreboard` green.
- [x] **Warnings as errors**: strict preset (`-DWERROR=ON`, Debug) builds. One
      warning remains in fetched third-party code
      (`_deps/bytetrack-src/src/lapjv.cpp:155`, unused parameter `y`) that
      predates this branch and carries none of our warning flags.
- [x] **Library-only build**: `cmake --workflow --preset lib-only` green
      (trackers, mask_overlap, sam3). `objdump -p libtrackers.so` shows only
      the pre-existing NEEDED entries (bytetrack, glog, onnxruntime,
      opencv, stdc++): no new inference library.
- [x] **Full suite passes**: see the final scoreboard run below.
- [x] **Formatting**: `clang-format --dry-run --Werror` clean on every
      changed/new C++ file.

### Behavioural assertions

All in the new `sam3` suite (`tests/test_sam3.cpp` + `ScriptedPropagator`),
plus the `track_config` additions. `PASS sam3`, `PASS track_config`.

- [x] **A detection births exactly one object** (`testBirthAndIdentity`).
- [x] **Propagation holds identity where boxes do not**
      (`testPropagationHoldsIdentityWhereBoxesDoNot`, at weights 0 and 1).
- [x] **`min_hits` gates output** (`testMinHitsGatesOutput`,
      `testBirthAndIdentity`).
- [x] **Absent resumes** (`testAbsentResumes`).
- [x] **Absent too long is lost** (`testAbsentTooLongIsLost`).
- [x] **Unsupported masklets die** (`testUnsupportedMaskletsDie`, positive
      and negative).
- [x] **Drift re-prompts the same id** (`testDriftReprompts`, plus the
      aligned control).
- [x] **Capacity is respected** (`testCapacityRespected`; warning-once is by
      inspection — the suite asserts the two births, the untracked third
      detection, and no crash).
- [x] **Both overloads** (`testBothOverloads`: identical output at weight 0).
- [x] **Class filter** (`testClassFilter`).
- [x] **Empty frame is an error** (`testClassFilter`: `std::invalid_argument`
      on both overloads).
- [x] **No detections, no objects** (`testNoDetectionsNoObjects`).

### Configuration

In `track_config`: `testCanonicalNames` (SAM3/SAM3.1/SAM-3.1/SAM31/sam3, all
existing spellings unchanged), `testNewTrackerDefaults` (SAM3 block),
`testOverridesWin` (all five `sam_*`), `testOverrideValidation` (ranges +
`sam_max_objects >= 1`).

- [x] `canonicalTrackerName` mappings.
- [x] `makeTrackConfig` SAM3 defaults and overrides.
- [x] `--tracker=SAM3` fails at startup with a message saying the propagator
      is unavailable, verified through the real binary on a replay with video:
      `Error during initialization: SAM3 tracking is unavailable in this
      build: it needs a MaskPropagator ... (specs/.../plan.md Group 4)`.
      The no-video replay is rejected earlier with
      `SAM3 propagates masks from the frame pixels, so replaying it also
      needs --source.`

### Manual

- [x] `--help` shows every `sam_*` flag with its default.
- [ ] Upstream issue (Group 0) opened and linked here. **Not executed**: no
      SAM / promptable / video-segmentation task exists in neuriplo-tasks as
      of 2026-11-10 (sibling at `0.8.6-dev` checked); filing the issue is a
      maintainer action and was left for one.
- [ ] End-to-end run with a SAM 3.1 export. **Not executed**, as expected:
      the upstream task does not exist yet.
- [ ] Missing `--sam_model` file naming the path. **Deferred with Group 4**:
      nothing reads the path until the propagator adapter exists; the flag is
      accepted and stored (`testOverridesWin` covers the plumbing).
- [x] Documentation links resolve (`AGENTS.md` hyperlink rule): README flag
      table, example, license note; Tracking_Algorithms §6; AGENTS.md seam and
      frame-required list; tech-stack rows; roadmap item.

### Diff review

- [x] No SORT / ByteTrack / BoTSORT / OC-SORT / C-BIoU behaviour change
      (no such file touched; all pre-existing suites still pass).
- [x] No inference library, ONNX session, or Python call under `trackers/`
      (`objdump` above; `Sam3Tracker` owns only the seam).
- [x] No weights, ONNX files, or export scripts committed.
- [x] The SAM License and gated access are stated in README, Tracking_
      Algorithms §6, and the roadmap item.
- [x] No document calls this McByte.
- [x] Nothing from the Out of Scope list appears in the diff.

### Definition of Done

- [x] Every In Scope item implemented; Group 4 deferred because Group 0 has
      not landed, and the roadmap says so.
- [x] `specs/roadmap.md` reflects what shipped.
- [x] Spec, code, and docs tell the same story.
