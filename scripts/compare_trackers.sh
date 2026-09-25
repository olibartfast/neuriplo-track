#!/usr/bin/env bash
#
# Replay one captured detection file through every tracker and print one table.
#
# This is the regression signal the roadmap asked for: the same detections
# through every tracker, so a tracker change can be shown to help or hurt
# instead of being eyeballed in a rendered video.
#
#   scripts/compare_trackers.sh --detections det.txt
#   scripts/compare_trackers.sh --detections det.txt --gt gt.txt
#   scripts/compare_trackers.sh --detections det.txt --trackers SORT,OCSORT
#
# Capture a detection file first:
#
#   neuriplo-track --source clip.mp4 --type yolov8 --weights m.onnx \
#       --labels coco.names --classes person --save_detections det.txt
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

DETECTIONS=""
GROUND_TRUTH=""
SOURCE=""
TRACKERS="SORT,ByteTrack,OCSORT,CBIoU,BoTSORT"
OUTPUT_DIR=""
APP=""
REID_ONNX=""

usage() {
    sed -n '3,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    cat <<'USAGE'
Options:
  --detections FILE   captured detections to replay (required)
  --gt FILE           ground truth; without it only label-free counts are shown
  --source FILE       video, needed only for BoTSORT's Re-ID features
  --reid-onnx FILE    Re-ID model, needed only for BoTSORT
  --trackers LIST     comma-separated (default: SORT,ByteTrack,OCSORT,CBIoU,BoTSORT)
  --output-dir DIR    where per-tracker MOT files go (default: a temporary dir)
  --app PATH          the neuriplo-track binary (default: the first one found)
  -h, --help          this message
USAGE
}

# Both --opt value and --opt=value are accepted, because the binary this drives
# takes the --opt=value form and it would be a trap for the two to disagree.
while [[ $# -gt 0 ]]; do
    case "$1" in
        --*=*)
            option="${1%%=*}"
            set -- "$option" "${1#*=}" "${@:2}"
            continue
            ;;
        --detections) DETECTIONS="$2"; shift 2 ;;
        --gt)         GROUND_TRUTH="$2"; shift 2 ;;
        --source)     SOURCE="$2"; shift 2 ;;
        --trackers)   TRACKERS="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --app)        APP="$2"; shift 2 ;;
        --reid-onnx)  REID_ONNX="$2"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z "$DETECTIONS" ]]; then
    echo "--detections is required." >&2
    usage >&2
    exit 2
fi
if [[ ! -f "$DETECTIONS" ]]; then
    echo "Detection file not found: $DETECTIONS" >&2
    exit 2
fi

# Locate the binary. The preset build tree is preferred because that is the one
# the scoreboard builds; the legacy build/ layout still works.
if [[ -z "$APP" ]]; then
    for candidate in \
        "$REPO_ROOT/build/scoreboard/app/neuriplo-track" \
        "$REPO_ROOT/build/app/neuriplo-track" \
        "$REPO_ROOT/build/neuriplo-track"
    do
        if [[ -x "$candidate" ]]; then APP="$candidate"; break; fi
    done
fi
if [[ -z "$APP" || ! -x "$APP" ]]; then
    echo "neuriplo-track binary not found. Build it with:" >&2
    echo "  cmake --workflow --preset scoreboard" >&2
    echo "or pass --app PATH." >&2
    exit 2
fi

if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$(mktemp -d)"
    trap 'rm -rf "$OUTPUT_DIR"' EXIT
else
    mkdir -p "$OUTPUT_DIR"
fi

echo "Binary:     $APP"
echo "Detections: $DETECTIONS"
[[ -n "$GROUND_TRUTH" ]] && echo "Ground truth: $GROUND_TRUTH"
[[ -n "$SOURCE" ]] && echo "Source:     $SOURCE"
echo "Results:    $OUTPUT_DIR"
echo

# Descriptive counts, computed from a MOT file alone. These need no labels and
# are NOT accuracy: a tracker that fragments every identity can still report a
# flattering "tracks created".
describe() {
    awk -F, '
        { rows++; ids[$2]++; per_frame[$1]++ }
        END {
            n = 0; for (id in ids) n++
            max = 0; for (f in per_frame) if (per_frame[f] > max) max = per_frame[f]
            if (n == 0) { printf "%8s %10s %10s", "0", "-", "0"; exit }
            printf "%8d %10.1f %10d", n, rows / n, max
        }
    ' "$1"
}

printf "%-12s %8s %10s %10s %10s\n" "TRACKER" "TRACKS" "MEAN_LEN" "MAX_CONC" "STATUS"
printf "%-12s %8s %10s %10s %10s\n" "------------" "--------" "----------" "----------" "----------"

FAILURES=0
IFS=',' read -ra TRACKER_LIST <<< "$TRACKERS"
for tracker in "${TRACKER_LIST[@]}"; do
    tracker="$(echo "$tracker" | tr -d '[:space:]')"
    [[ -z "$tracker" ]] && continue

    out="$OUTPUT_DIR/${tracker}.txt"
    args=(--detections="$DETECTIONS" --tracker="$tracker" --mot_output="$out")

    # BoTSORT extracts Re-ID features from the frame, so it cannot replay
    # without a video. Reported rather than silently dropped.
    if [[ "${tracker,,}" == "botsort" || "${tracker,,}" == "bot-sort" ]]; then
        if [[ -z "$SOURCE" || -z "$REID_ONNX" ]]; then
            printf "%-12s %8s %10s %10s %10s\n" "$tracker" "-" "-" "-" "skipped"
            echo "             BoTSORT needs --source and --reid-onnx for Re-ID features." >&2
            continue
        fi
        args+=(--source="$SOURCE" --reid_onnx="$REID_ONNX")
    fi

    if ! "$APP" "${args[@]}" >"$OUTPUT_DIR/${tracker}.log" 2>&1; then
        printf "%-12s %8s %10s %10s %10s\n" "$tracker" "-" "-" "-" "FAILED"
        FAILURES=$((FAILURES + 1))
        continue
    fi

    printf "%-12s " "$tracker"
    describe "$out"
    printf " %10s\n" "ok"
done

echo
echo "TRACKS   distinct track ids emitted"
echo "MEAN_LEN mean rows per track id"
echo "MAX_CONC most tracks reported in any one frame"
echo "These are descriptive counts, not accuracy. Pass --gt for MOTA / IDF1 / IDsw."

if [[ -n "$GROUND_TRUTH" ]]; then
    echo
    for tracker in "${TRACKER_LIST[@]}"; do
        tracker="$(echo "$tracker" | tr -d '[:space:]')"
        out="$OUTPUT_DIR/${tracker}.txt"
        [[ -f "$out" ]] || continue
        echo "=============================================================="
        echo "$tracker"
        echo "=============================================================="
        python3 "$REPO_ROOT/scripts/eval_mot.py" \
            --gt "$GROUND_TRUTH" --tracker-output "$out" --name "$tracker" || true
    done
fi

exit $(( FAILURES > 0 ? 1 : 0 ))
