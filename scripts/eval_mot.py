#!/usr/bin/env python3
"""Score a MOTChallenge tracking output against ground truth.

Reachable from a documented command, which is the point: the vendored
``trackers/BoTSORT/src/mot_metrics_evaluator.py`` computes similar numbers but
is not wired into any project workflow, and is left untouched.

    ./build/scoreboard/app/neuriplo-track --detections det.txt \
        --tracker=OCSORT --mot_output tracks.txt
    python3 scripts/eval_mot.py --gt gt.txt --tracker-output tracks.txt

Both files are MOTChallenge rows:
``frame,id,bb_left,bb_top,bb_width,bb_height,conf,...`` with 1-based frames --
the same format ``app/src/MotFile.cpp`` writes.
"""

import argparse
import importlib.util
import sys
from collections import defaultdict

MISSING_DEPENDENCY = (
    "py-motmetrics is not installed.\n"
    "  pip install motmetrics\n"
    "It is an evaluation-only dependency: nothing in the C++ build or the test\n"
    "suite needs it, which is why it is not installed by\n"
    "scripts/setup_dependencies.sh."
)

# What the numbers mean, printed above every summary so a figure cannot be
# quoted out of context.
PREAMBLE = """\
These numbers are a regression signal for changes in this repository: the same
detections through two trackers, or one tracker before and after a change.

They are NOT MOTChallenge results. This harness does not handle distractor
classes, does not use the visibility column, and does not apply the benchmark's
preprocessing. Comparing them to published figures is meaningless.

HOTA is not computed here. It needs TrackEval
(https://github.com/JonathonLuiten/TrackEval), which consumes exactly the files
this tool reads -- so the route is: produce the MOT output, then point TrackEval
at it.
"""


def parse_args():
    parser = argparse.ArgumentParser(
        description="MOTA / IDF1 / IDsw over a MOTChallenge tracking output.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--gt", required=True, help="ground-truth file, MOTChallenge format")
    parser.add_argument(
        "--tracker-output", required=True, help="tracking output to score, MOTChallenge format"
    )
    parser.add_argument(
        "--name", default=None, help="label for the summary row (default: the tracker-output filename)"
    )
    parser.add_argument(
        "--max-iou-distance",
        type=float,
        default=0.5,
        help="maximum IoU distance for a match, i.e. 1 - IoU (default: 0.5, the MOTChallenge convention)",
    )
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="parse both files, report their shape, and exit -- needs no py-motmetrics",
    )
    parser.add_argument(
        "--min-confidence",
        type=float,
        default=None,
        help="drop tracker rows whose confidence column is below this (default: keep all)",
    )
    return parser.parse_args()


def read_mot(path, min_confidence=None):
    """frame -> (ids, boxes). Mirrors readMotFile()'s tolerance: at least seven
    columns, extra ones ignored, blank lines skipped, 1-based frames."""
    by_frame = defaultdict(lambda: ([], []))
    with open(path, "r", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, start=1):
            text = line.strip()
            if not text:
                continue
            fields = text.split(",")
            if len(fields) < 7:
                raise SystemExit(
                    f"{path}:{line_number}: expected at least 7 comma-separated columns, found {len(fields)}"
                )
            try:
                frame = int(float(fields[0]))
                object_id = int(float(fields[1]))
                x, y, w, h = (float(v) for v in fields[2:6])
                confidence = float(fields[6])
            except ValueError as error:
                raise SystemExit(f"{path}:{line_number}: {error}") from error
            if frame < 1:
                raise SystemExit(f"{path}:{line_number}: frame numbers are 1-based, found {frame}")
            if min_confidence is not None and confidence < min_confidence:
                continue
            ids, boxes = by_frame[frame]
            ids.append(object_id)
            boxes.append([x, y, w, h])
    return by_frame


def main():
    args = parse_args()

    if args.check_only:
        ground_truth = read_mot(args.gt)
        tracked = read_mot(args.tracker_output, args.min_confidence)
        for label, path, parsed in (
            ("ground truth", args.gt, ground_truth),
            ("tracker output", args.tracker_output, tracked),
        ):
            rows = sum(len(ids) for ids, _ in parsed.values())
            frames = sorted(parsed)
            span = f"{frames[0]}..{frames[-1]}" if frames else "empty"
            unique = len({i for ids, _ in parsed.values() for i in ids})
            print(f"{label:>14}: {path}")
            print(f"{'':>14}  {rows} rows, {len(frames)} frames ({span}), {unique} distinct ids")
        return

    # Checked before the files are read, so a missing dependency is reported
    # immediately rather than after parsing; imported only after they are read,
    # so a malformed file is still diagnosed on a machine without motmetrics.
    if importlib.util.find_spec("motmetrics") is None:
        sys.exit(MISSING_DEPENDENCY)

    ground_truth = read_mot(args.gt)
    tracked = read_mot(args.tracker_output, args.min_confidence)

    try:
        import motmetrics as mm
        import numpy as np
    except ImportError:
        sys.exit(MISSING_DEPENDENCY)

    if not ground_truth:
        sys.exit(f"{args.gt} contains no rows; there is nothing to score against.")
    if not tracked:
        sys.exit(f"{args.tracker_output} contains no rows; the tracker produced no output.")

    print(PREAMBLE)

    accumulator = mm.MOTAccumulator(auto_id=False)

    # Every frame either file mentions, so a frame the tracker missed entirely
    # still counts as false negatives rather than vanishing from the total.
    frames = sorted(set(ground_truth) | set(tracked))
    for frame in frames:
        gt_ids, gt_boxes = ground_truth.get(frame, ([], []))
        t_ids, t_boxes = tracked.get(frame, ([], []))
        distances = mm.distances.iou_matrix(
            np.array(gt_boxes).reshape(-1, 4),
            np.array(t_boxes).reshape(-1, 4),
            max_iou=args.max_iou_distance,
        )
        accumulator.update(gt_ids, t_ids, distances, frameid=frame)

    metrics = mm.metrics.create()
    name = args.name or args.tracker_output
    summary = metrics.compute(
        accumulator,
        metrics=[
            "mota",
            "motp",
            "idf1",
            "idp",
            "idr",
            "precision",
            "recall",
            "mostly_tracked",
            "partially_tracked",
            "mostly_lost",
            "num_false_positives",
            "num_misses",
            "num_switches",
            "num_fragmentations",
            "num_unique_objects",
        ],
        name=name,
    )

    print(
        mm.io.render_summary(
            summary,
            formatters=metrics.formatters,
            namemap={
                "mota": "MOTA",
                "motp": "MOTP",
                "idf1": "IDF1",
                "idp": "IDP",
                "idr": "IDR",
                "precision": "Prcn",
                "recall": "Rcll",
                "mostly_tracked": "MT",
                "partially_tracked": "PT",
                "mostly_lost": "ML",
                "num_false_positives": "FP",
                "num_misses": "FN",
                "num_switches": "IDsw",
                "num_fragmentations": "FM",
                "num_unique_objects": "GT",
            },
        )
    )
    print(f"\nFrames scored: {len(frames)}  (ground truth {len(ground_truth)}, tracker {len(tracked)})")


if __name__ == "__main__":
    main()
