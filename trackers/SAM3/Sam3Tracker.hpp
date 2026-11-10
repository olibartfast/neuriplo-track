//
// Sam3Tracker.hpp: detector-prompted tracker loop over a MaskPropagator.
//
// Identity is carried by propagating each object's mask instead of predicting
// boxes with a motion model. The detector still decides what exists: every
// frame, live objects are propagated, present ones are associated to the
// frame's detections with Hungarian assignment on 1 - IoU (box IoU blended
// with mask IoU exactly as OC-SORT and C-BIoU do), matches re-prompt on drift,
// unmatched detections birth new objects up to capacity, and objects without a
// detection match for more than max_age frames are removed. Output is one box
// per present object with enough detection matches, taken from its propagated
// mask, with the confidence of its last matched detection.
//
// Absent (occluded) objects produce no output but keep their id and memory
// for up to max_age frames, so re-appearance resumes the same track. A masklet
// the detector never supports is removed all the same: the detector is the
// arbiter of what exists, the propagator the arbiter of where it is.
//
#pragma once
#include "MaskPropagator.hpp"

#include <memory>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>

namespace sam3 {

struct DetectionBox {
    cv::Rect_<float> box;
    float score{};
    int class_id{};
    tracking::MaskRegion mask; // empty unless the detector is a segmentation model
};

struct TrackBox {
    int id{};
    cv::Rect_<float> box;
    float score{};
    int class_id{};
};

class Sam3Tracker {
  public:
    // max_age: consecutive frames without a detection match before a live
    //   object is removed, present or not.
    // min_hits: detection matches before an object is reported.
    // iou_threshold: association gate on the blended overlap.
    // presence_thresh: propagated objects below this presence are absent:
    //   kept, but neither matched nor reported.
    // det_thresh: detections below this score are ignored entirely.
    // reprompt_iou: a matched pair whose propagated and detection boxes agree
    //   less than this is fed back as a correction prompt, keeping the id.
    // max_objects: bound on live objects; non-positive means the propagator's
    //   own capacity. New detections past the bound are untracked.
    // mask_iou_weight: blend of mask overlap into the association cost.
    // propagator: owned inference seam; must not be null.
    Sam3Tracker(int max_age = 30, int min_hits = 3, float iou_threshold = 0.3f, float presence_thresh = 0.5f,
                float det_thresh = 0.5f, float reprompt_iou = 0.5f, int max_objects = 64, float mask_iou_weight = 0.0f,
                std::unique_ptr<tracking::MaskPropagator> propagator = nullptr);

    // Advances one frame. The frame pixels are mandatory: without them there
    // is nothing to propagate onto.
    std::vector<TrackBox> update(const std::vector<DetectionBox> &detections, const cv::Mat &frame);

  private:
    struct Track {
        int prop_id{};
        int missed{};  // consecutive frames without a detection match
        int hits{};    // detection matches so far
        float score{}; // last matched detection's score
        int class_id{};
        bool present{};            // this frame's propagated presence
        tracking::MaskRegion mask; // this frame's propagated mask
    };

    int effectiveCapacity() const;

    int max_age_;
    int min_hits_;
    float iou_threshold_;
    float presence_thresh_;
    float det_thresh_;
    float reprompt_iou_;
    int max_objects_;
    float mask_iou_weight_;
    std::unique_ptr<tracking::MaskPropagator> propagator_;
    std::vector<Track> tracks_;
    bool capacity_warned_{};
};

} // namespace sam3
