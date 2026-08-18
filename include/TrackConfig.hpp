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
