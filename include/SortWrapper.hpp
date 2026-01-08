#pragma once
#include "BaseTracker.hpp"
#include "Sort.hpp"
#include <memory>

class SortWrapper : public BaseTracker {
private:
  std::unique_ptr<Sort> tracker_;

public:
  explicit SortWrapper(const TrackConfig &config);
  ~SortWrapper() override;

  std::vector<TrackedObject>
  update(const std::vector<vision_core::Detection> &detections,
         const cv::Mat &frame = cv::Mat()) override;
};
