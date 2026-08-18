#pragma once
#include "BaseTracker.hpp"
#include "OCSort.hpp"

#include <memory>

class OCSortWrapper : public BaseTracker {
  private:
    std::unique_ptr<ocsort::OCSort> tracker_;

  public:
    explicit OCSortWrapper(const TrackConfig &config);
    ~OCSortWrapper() override;

    std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::Detection> &detections,
                                      const cv::Mat &frame = cv::Mat()) override;
};
