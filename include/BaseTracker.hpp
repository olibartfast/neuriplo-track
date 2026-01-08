#pragma once
#include "TrackConfig.hpp"
#include "TrackedObject.hpp"
#include <opencv2/core/types.hpp>
#include <vector>
#include <vision-core/core/result_types.hpp>

class BaseTracker {
protected:
  TrackConfig config_;

public:
  explicit BaseTracker(const TrackConfig &config) : config_(config) {}
  virtual ~BaseTracker() = default;
  virtual std::vector<TrackedObject>
  update(const std::vector<vision_core::Detection> &detections,
         const cv::Mat &frame = cv::Mat()) = 0;
};
