#pragma once
#include "BaseTracker.hpp"
#include "Sam3Tracker.hpp"

#include <memory>

// Detector-prompted mask-propagation tracker behind the MaskPropagator seam.
//
// Both update() overloads run the same loop: the Detection one associates on
// boxes alone, the InstanceSegmentation one additionally feeds each detection's
// mask into the association cue when TrackConfig::mask_iou_weight is non-zero.
// The frame is mandatory — masks are propagated from its pixels — so an empty
// frame is an error rather than a silent no-op.
class Sam3Wrapper : public BaseTracker {
  private:
    std::unique_ptr<sam3::Sam3Tracker> tracker_;

  public:
    // The propagator is the inference seam (an adapter over the upstream SAM
    // 3.1 task once it lands, a scripted double in tests). It must not be
    // null; the tracker loop owns it from here on.
    Sam3Wrapper(const TrackConfig &config, std::unique_ptr<tracking::MaskPropagator> propagator);
    ~Sam3Wrapper() override;

    std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::Detection> &detections,
                                      const cv::Mat &frame = cv::Mat()) override;

    std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::InstanceSegmentation> &segmentations,
                                      const cv::Mat &frame = cv::Mat()) override;
};
