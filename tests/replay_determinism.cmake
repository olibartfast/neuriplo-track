# Drives the replay path end to end: no model, no inference engine, no backend
# and no video. Invoked by ctest as
#   cmake -DAPP=<binary> -DWORKDIR=<dir> -P replay_determinism.cmake
#
# Checks that a replay runs at all without a detector, that it is deterministic
# (the same file replayed twice is byte-identical), and that the tracker is
# genuinely being stepped -- an empty result would otherwise pass trivially.

if(NOT DEFINED APP OR NOT DEFINED WORKDIR)
    message(FATAL_ERROR "APP and WORKDIR must be set")
endif()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

# Two objects translating steadily for ten frames, written as a captured
# detection file: id -1, 1-based frames, confidence in the seventh column.
set(DETECTIONS "")
foreach(frame RANGE 1 10)
    math(EXPR left_x "10 + (${frame} - 1) * 12")
    math(EXPR right_x "400 - (${frame} - 1) * 12")
    string(APPEND DETECTIONS "${frame},-1,${left_x},100,50,80,0.9,-1,-1,-1\n")
    string(APPEND DETECTIONS "${frame},-1,${right_x},260,50,80,0.85,-1,-1,-1\n")
endforeach()

set(DET_FILE "${WORKDIR}/det.txt")
file(WRITE "${DET_FILE}" "${DETECTIONS}")

function(run_replay tracker output)
    execute_process(
        COMMAND "${APP}" --detections=${DET_FILE} --tracker=${tracker} --mot_output=${output}
        WORKING_DIRECTORY "${WORKDIR}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "replay with ${tracker} failed (${status})\n${out}\n${err}")
    endif()
endfunction()

# A replay must not need a detector, a model file or a labels file.
run_replay(OCSORT "${WORKDIR}/ocsort-a.txt")
run_replay(OCSORT "${WORKDIR}/ocsort-b.txt")

if(NOT EXISTS "${WORKDIR}/ocsort-a.txt")
    message(FATAL_ERROR "replay produced no MOT output")
endif()

# Not empty: the tracker really ran, rather than the file parsing to nothing.
file(READ "${WORKDIR}/ocsort-a.txt" FIRST_RUN)
string(LENGTH "${FIRST_RUN}" FIRST_RUN_LENGTH)
if(FIRST_RUN_LENGTH EQUAL 0)
    message(FATAL_ERROR "replay produced an empty MOT file; the tracker was never stepped")
endif()

# Byte-identical across runs: the replay path carries no hidden state.
file(READ "${WORKDIR}/ocsort-b.txt" SECOND_RUN)
if(NOT FIRST_RUN STREQUAL SECOND_RUN)
    message(FATAL_ERROR "replaying the same detections twice produced different output")
endif()

# 1-based frame numbering, and never beyond the frames the file describes.
string(REPLACE "\n" ";" LINES "${FIRST_RUN}")
foreach(line IN LISTS LINES)
    if(line STREQUAL "")
        continue()
    endif()
    string(REPLACE "," ";" FIELDS "${line}")
    list(GET FIELDS 0 FRAME)
    if(FRAME LESS 1 OR FRAME GREATER 10)
        message(FATAL_ERROR "MOT frame number ${FRAME} outside the replayed range 1..10: ${line}")
    endif()
endforeach()

# Every tracker must survive the same replay.
foreach(tracker SORT ByteTrack CBIoU)
    run_replay(${tracker} "${WORKDIR}/${tracker}.txt")
endforeach()

# A replay that asks for the mask cue is rejected, not silently ignored.
execute_process(
    COMMAND "${APP}" --detections=${DET_FILE} --tracker=OCSORT --mask_iou_weight=0.5
            --mot_output=${WORKDIR}/masked.txt
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE masked_status
    OUTPUT_QUIET ERROR_QUIET)
if(masked_status EQUAL 0)
    message(FATAL_ERROR "--mask_iou_weight should be rejected on a replay, but the run succeeded")
endif()

# So is a replay that also names a detector.
execute_process(
    COMMAND "${APP}" --detections=${DET_FILE} --tracker=OCSORT --weights=nonexistent.onnx
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE weights_status
    OUTPUT_QUIET ERROR_QUIET)
if(weights_status EQUAL 0)
    message(FATAL_ERROR "--weights should be rejected on a replay, but the run succeeded")
endif()

message(STATUS "PASS replay")
