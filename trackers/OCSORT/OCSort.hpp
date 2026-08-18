//
// OCSort.hpp: OC-SORT (Observation-Centric SORT) class declaration.
//
// Cao et al., "Observation-Centric SORT: Rethinking SORT for Robust
// Multi-Object Tracking" (https://arxiv.org/abs/2203.14360).
//
// SORT degrades when a track is lost, because the Kalman filter keeps
// integrating its own predictions and the estimate drifts. OC-SORT leans on the
// observations instead:
//   ORU - on re-association, the filter is replayed over a virtual trajectory
//         between the last real observation and the new one.
//   OCM - association also scores the consistency between a track's direction of
//         travel and the direction to the candidate detection.
//   OCR - a second association round matches leftover detections against the
//         last observations of the tracks that stayed unmatched.
//
#pragma once
#include "MaskOverlap.hpp"
#include "OCSortKalmanTracker.hpp"

#include <memory>
#include <opencv2/core/types.hpp>
#include <vector>

namespace ocsort {

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

class OCSort {
  public:
    OCSort(int max_age = 30, int min_hits = 3, float iou_threshold = 0.3f, int delta_t = 3, float inertia = 0.2f,
           float det_thresh = 0.6f, float mask_iou_weight = 0.0f);

    std::vector<TrackBox> update(const std::vector<DetectionBox> &detections);

  private:
    // First association: IoU plus the OCM direction-consistency term.
    void associateWithMomentum(const std::vector<DetectionBox> &detections,
                               const std::vector<cv::Rect_<float>> &predictions, std::vector<int> &detection_for_track,
                               std::vector<bool> &detection_matched) const;

    // OCR: leftover detections against the last observations of unmatched tracks.
    void associateWithObservations(const std::vector<DetectionBox> &detections, std::vector<int> &detection_for_track,
                                   std::vector<bool> &detection_matched) const;

    int max_age_;
    int min_hits_;
    float iou_threshold_;
    int delta_t_;
    float inertia_;
    float det_thresh_;
    float mask_iou_weight_;
    int frame_count_{};

    std::vector<std::unique_ptr<OCSortKalmanTracker>> trackers_;
};

} // namespace ocsort
