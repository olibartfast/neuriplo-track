#pragma once
#include "BaseTracker.hpp"
#include "ByteTrack/BYTETracker.h"
#include <memory>

class ByteTrackWrapper : public BaseTracker {
private:
  std::unique_ptr<byte_track::BYTETracker> tracker_;

public:
  explicit ByteTrackWrapper(const TrackConfig &config);
  ~ByteTrackWrapper() override;

  std::vector<TrackedObject>
  update(const std::vector<vision_core::Detection> &detections,
         const cv::Mat &frame = cv::Mat()) override;
};
