#pragma once
#include <set>
#include <string>

struct TrackConfig {
    std::set<int> classes_to_track;

    // SORT parameters
    int max_age = 1;
    int min_hits = 3;
    float iou_threshold = 0.3f;

    // ByteTrack parameters
    int track_buffer = 30;
    float track_thresh = 0.5f;
    float high_thresh = 0.6f;
    float match_thresh = 0.8f;

    // OC-SORT parameters (https://arxiv.org/abs/2203.14360)
    // delta_t: how many frames back the observation used for the velocity
    // direction (OCM) is taken from.
    // inertia: weight of the direction-consistency term in the association cost.
    // det_thresh: detections below this score do not start or update a track.
    int ocsort_delta_t = 3;
    float ocsort_inertia = 0.2f;
    float ocsort_det_thresh = 0.6f;

    // C-BIoU parameters (https://arxiv.org/abs/2211.14317)
    // b1 / b2: buffer scales for the first and second cascaded matching rounds.
    // motion_n: number of past observations averaged by the motion model.
    float cbiou_b1 = 0.3f;
    float cbiou_b2 = 0.5f;
    int cbiou_motion_n = 5;

    // Mask-conditioned association (OC-SORT, C-BIoU). 0 disables the cue and
    // reproduces pure box-overlap association; 1 associates on masks alone.
    // Only has an effect when the detector is a segmentation model.
    float mask_iou_weight = 0.0f;

    // SAM3 parameters (specs/2026-10-10-sam3-video-tracker/requirements.md).
    // presence_thresh: propagated objects below this presence are absent.
    // det_thresh: detections below this score never prompt the propagator.
    // reprompt_iou: a matched pair agreeing less than this is fed back as a
    //   correction prompt, keeping the id.
    // max_objects: bound on live objects, a multiple of the 16-object
    //   multiplex bucket; non-positive means the propagator's own capacity.
    // model_path: SAM 3.1 export consumed by the app-side propagator adapter
    //   (Group 4); unset until that adapter exists.
    float sam_presence_thresh = 0.5f;
    float sam_det_thresh = 0.5f;
    float sam_reprompt_iou = 0.5f;
    int sam_max_objects = 64;
    std::string sam_model_path;

    // BoTSORT parameters
    std::string tracker_config_path;
    std::string gmc_config_path;
    std::string reid_config_path;
    std::string reid_onnx_path;

    TrackConfig(const std::set<int> &classes = {}, const std::string &trackerPath = "", const std::string &gmcPath = "",
                const std::string &reidPath = "", const std::string &onnxPath = "")
        : classes_to_track(classes), tracker_config_path(trackerPath), gmc_config_path(gmcPath),
          reid_config_path(reidPath), reid_onnx_path(onnxPath) {}
};
