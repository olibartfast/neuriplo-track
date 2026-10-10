# Feature Requirements — SAM 3.1 as a video tracker

> **Historical record.** Written before the work, delivered in PR TBD, and not
> maintained afterwards. For what is true today see the constitution in
> [../mission.md](../mission.md), [../tech-stack.md](../tech-stack.md) and
> [../roadmap.md](../roadmap.md), or [specs/README.md](../README.md) for how
> these documents relate.

Roadmap phase: [Phase 5 — Capability](../roadmap.md), the "SAM 3.1 video
tracker" item. It also delivers the propagation seam that the "Remaining
trackers" item records as McByte's blocker.

## Goal

Track objects by **propagating their masks through the video**, rather than by
predicting boxes with a motion model and matching them to the next frame's
detections. The detector still decides *what* is tracked; SAM 3.1 decides
*where it went*.

Every tracker in this repository today carries identity with a Kalman filter or
a displacement model plus box (and optionally per-frame mask) overlap. All of
them lose identity the same way: two objects whose boxes cross, merge, or
disappear for longer than the motion model can extrapolate. A video segmenter
with an appearance memory does not depend on the box geometry staying
unambiguous, so it addresses exactly the cases the mask cue from
[2026-08-18-mask-association](../2026-08-18-mask-association/requirements.md)
could not, because that cue was per-frame and never propagated.

### What SAM 3.1 is

Facts this packet relies on, checked on 2026-10-10 against the public
[facebookresearch/sam3](https://github.com/facebookresearch/sam3) repository
and secondary write-ups:

- SAM 3 (Meta, Nov 2025) is a **detector and a tracker sharing one vision
  encoder**. The detector is DETR-style and conditioned on text, geometry or
  exemplar prompts; the tracker is the SAM 2 lineage — a memory bank of past
  frames, a mask decoder, and a per-object presence (occlusion) score.
- SAM 3.1 (released 2026-03-27) is a drop-in checkpoint update to the tracker
  side. Its **Object Multiplex** groups objects into fixed-capacity buckets
  (16 for the public checkpoint) that share one forward pass and one spatial
  memory, instead of one pass per object. Meta reports ~7× faster inference at
  128 objects with no accuracy loss. The backbone and prompt types are
  unchanged.
- Weights are gated on Hugging Face (`facebook/sam3.1`) under the **SAM
  License**, not an OSI licence.
- There is **no official ONNX export**. Community exports exist and split the
  model into several graphs (image encoder, detector, memory attention / mask
  decoder, memory encoder); they report the tracker attention and rotary
  position handling as the export-sensitive parts.

## In Scope

- **A propagation seam in the tracker library.**
  `trackers/common/MaskPropagator.hpp`: an abstract interface that advances
  every live object onto the next frame and returns, per object id, a mask
  (`MaskRegion`, reusing `trackers/common/MaskOverlap`) and a presence score;
  plus calls to add an object from a box prompt on the current frame, to
  re-prompt an existing object, and to remove one. Nothing SAM-specific
  appears in it.
- **`Sam3Tracker`** (`trackers/SAM3/`), implementing the detector-prompted
  loop over a `MaskPropagator`:
  1. propagate all live objects onto the frame;
  2. objects whose presence score is below `sam_presence_thresh` are *absent*
     this frame (occluded) and produce no output;
  3. associate this frame's detections (class-filtered, score ≥
     `sam_det_thresh`) to present objects with Hungarian assignment on
     `1 − IoU`, where IoU is between the detection box and the propagated
     mask's box — blended with mask IoU via `mask_iou_weight` when the
     detection carries a mask, exactly as OC-SORT and C-BIoU already do;
  4. a matched object whose propagated box has drifted below
     `sam_reprompt_iou` against its detection is re-prompted with that box,
     keeping its id;
  5. an unmatched detection starts a new object (box prompt, new id) if the
     propagator has free capacity;
  6. an object neither present nor supported by a detection for more than
     `max_age` consecutive frames is removed from the propagator;
  7. output one `TrackedObject` per present object that has `min_hits`
     detection matches, box taken from the propagated mask, confidence from
     its last matched detection.
- **`Sam3Wrapper`** implementing `BaseTracker`, both overloads: the
  `InstanceSegmentation` overload passes masks to the association cue, the
  `Detection` overload works without them.
- **Configuration**: `TrackConfig` fields `sam_presence_thresh`,
  `sam_det_thresh`, `sam_reprompt_iou`, `sam_max_objects`, `sam_model_path`;
  matching CLI flags; per-algorithm defaults in `makeTrackConfig`;
  `--tracker=SAM3` with `SAM3.1`, `SAM-3.1`, `SAM31` accepted by
  `canonicalTrackerName`.
- **An adapter from the seam to an upstream SAM 3.1 task**, constructed in
  `MultiObjectTrackingApp::createTracker` and injected into the wrapper — gated
  on the upstream prerequisite below.
- **Tests** against a scripted `MaskPropagator` — no weights, no video, as
  `AGENTS.md` requires of the suite.
- **Documentation**: `docs/Tracking_Algorithms.md`, `README.md`, `AGENTS.md`,
  `specs/tech-stack.md`, `specs/roadmap.md`, including the licence and the
  fact that no weights are shipped.

### Upstream prerequisite (not delivered here)

A stateful promptable video segmentation task in
[neuriplo-tasks](https://github.com/olibartfast/neuriplo-tasks), running the
SAM 3.1 tracker graphs through [neuriplo](https://github.com/olibartfast/neuriplo)
so the inference backend stays a CMake option. As of 2026-10-10 neuriplo-tasks
has no SAM, promptable segmentation or video object segmentation task. This
packet states the contract it needs (plan.md, Group 0) and does not implement
it. Groups 1–3 are deliverable and useful without it: integrators can supply
their own `MaskPropagator`.

## Out of Scope

- **Concept-prompted tracking** (`--prompt="person"`, SAM 3's detector
  replacing YOLO). That is a detector: it belongs in neuriplo-tasks next to the
  existing open-vocabulary detection (OWLv2, Grounding DINO), and once there it
  feeds *every* tracker, not only this one.
- **Running SAM inference in `trackers/`.** No ONNX Runtime session, Python
  bridge, or LibTorch call is added to the tracker library (decision 2).
- **McByte.** This branch provides the seam McByte needs; McByte itself —
  BoT-SORT association with a propagated mask as a cue and clear-match
  locking — is a separate packet.
- **`TrackedObject` carrying a mask** and mask visualisation. The output type's
  stability is a Phase 4 item; masks stay internal.
- **Shipping, downloading, or converting weights.** No checkpoint, ONNX file,
  or export script enters the repository; scripts take paths.
- **Real-time claims.** No latency target is set. The encoder runs on every
  frame; the Phase 5 "Performance instrumentation" item is how a number gets
  attached later.
- **SAM 2 / SAM 3.0 checkpoints as tested configurations.** The seam does not
  preclude them; only SAM 3.1 is validated.

## Decisions

1. **The detector stays the source of identity births.** SAM 3.1 can run
   concept-prompted on its own, but then it is a detector *and* a tracker, and
   the detection boundary in [mission.md](../mission.md) — model families vary
   behind `TaskFactory`, trackers consume `Detection` — would be bypassed.
   Prompting from detections keeps `--type`, `--weights` and `--classes`
   meaning what they mean for every other tracker, keeps the Phase 3 detection
   capture/replay usable, and keeps `Detection` the only type flowing in
   (principle 2).
2. **Inference lives upstream behind a seam, not in the tracker.** BoTSORT's
   Re-ID calls ONNX Runtime directly from `trackers/`, which ties it to one
   backend; that is the precedent *not* to follow. SAM 3.1 is several graphs
   with recurrent state, which is model-runtime work, and the mask-association
   packet already ruled that "a SAM or Cutie task belongs in neuriplo-tasks".
   The seam also keeps `BUILD_ONLY_LIB=ON` free of any inference dependency and
   makes the tracker logic testable with a scripted propagator.
3. **One tracker named `SAM3`, validated on the 3.1 checkpoint.** Object
   Multiplex is a property of the checkpoint and its export, not of the
   association loop; the loop is identical for 3.0. The canonical name follows
   the model family; `SAM3.1` and `SAM31` are accepted spellings, matching how
   `OC-SORT` is handled. `canonicalTrackerName` must strip `.` as a separator.
4. **Absent is not lost.** The presence score is SAM's own occlusion signal.
   An absent object is withheld from output but kept in memory for up to
   `max_age` frames, so re-appearance after occlusion resumes the same id —
   the case this tracker exists for. The default `max_age` is therefore long
   (30, as for OC-SORT / C-BIoU), not SORT's 1.
5. **Masklets need detector support to survive.** A propagated object with no
   matching detection for `max_age` frames is removed even if SAM still reports
   it present. Without this, a masklet that has drifted onto background lives
   for ever and is reported as a track. The detector is the arbiter of what
   exists; SAM is the arbiter of where it is.
6. **Re-prompt on drift, don't replace.** When a matched detection and the
   propagated box disagree below `sam_reprompt_iou`, the detection box is fed
   back as a correction prompt for the same id. Restarting the object would
   discard its memory and its id; ignoring the disagreement lets drift
   accumulate.
7. **Capacity is explicit.** `sam_max_objects` bounds live objects (default
   64, a multiple of the 16-object bucket). New detections beyond capacity are
   not tracked, with a single `LOG(WARNING)` per run rather than per frame.
   Silently degrading to a slower path or evicting a live object would both
   change identities in ways a user cannot see.
8. **The frame is mandatory.** Like BoTSORT, this tracker cannot run without
   pixels. An empty `cv::Mat` is an `std::invalid_argument`, not a silent
   no-op.
9. **Reuse the association cost already in the repo.** Box IoU blended with
   `maskIoU` by `mask_iou_weight` is the cost OC-SORT and C-BIoU use. A third
   formula would make the trackers harder to compare on the Phase 3 harness.

## Constraints and Context

- `trackers` is a SHARED library linking static `neuriplo-tasks`; anything new
  under `trackers/` depends only on OpenCV, Eigen and `neuriplo-tasks` types.
- `BaseTracker::update({})` is ambiguous since the mask overload; tests name
  the vector type.
- The SAM License and gated access mean CI will never hold weights. Every
  behavioural assertion must therefore be provable with the scripted
  propagator; the end-to-end run is a manual check and is expected to be
  recorded as **not executed** if the upstream task has not landed.
- The ONNX export is unofficial and not reproducible from a pinned source in
  this repository. If the upstream task fetches an exporter, it must be pinned
  there, per principle 4.
- Must build clean under `-Wall -Wextra -Wpedantic -Werror`, match
  `.clang-format`, and leave every existing tracker's output unchanged.
- The Phase 3 replay path (`--detections`) skips the detector but still needs
  the video, because this tracker reads frames. That must be documented, not
  worked around.
