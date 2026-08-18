#pragma once
#include "BaseTracker.hpp"
#include "CBIoUTracker.hpp"

#include <memory>

class CBIoUWrapper : public BaseTracker {
  private:
    std::unique_ptr<cbiou::CBIoUTracker> tracker_;

  public:
    explicit CBIoUWrapper(const TrackConfig &config);
    ~CBIoUWrapper() override;

    std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::Detection> &detections,
                                      const cv::Mat &frame = cv::Mat()) override;

    // Mask-aware path: identical association, with the mask cue available when
    // TrackConfig::mask_iou_weight is non-zero.
    std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::InstanceSegmentation> &segmentations,
                                      const cv::Mat &frame = cv::Mat()) override;
};
