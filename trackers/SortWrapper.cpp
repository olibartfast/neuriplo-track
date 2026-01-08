#include "SortWrapper.hpp"

SortWrapper::SortWrapper(const TrackConfig &config)
    : BaseTracker(config),
      tracker_(std::make_unique<Sort>(config.max_age, config.min_hits,
                                      config.iou_threshold)) {}

SortWrapper::~SortWrapper() = default;

std::vector<TrackedObject>
SortWrapper::update(const std::vector<vision_core::Detection> &detections,
                    const cv::Mat &frame) {
  std::vector<TrackedObject> tracksOutput;
  std::vector<TrackingBox> detectionsToTrack;

  // Filter detections based on class IDs and convert to TrackingBox
  for (const auto &detection : detections) {
    if (config_.classes_to_track.find(static_cast<int>(detection.class_id)) !=
        config_.classes_to_track.end()) {
      TrackingBox tb;
      tb.id = static_cast<int>(detection.class_id);
      tb.box = cv::Rect_<float>(detection.bbox);
      detectionsToTrack.push_back(tb);
    }
  }

  // Update tracker
  std::vector<TrackingBox> tracks = tracker_->update(detectionsToTrack);

  // Convert tracks to TrackedObject format
  for (const auto &track : tracks) {
    TrackedObject trackedObj;
    trackedObj.track_id = track.id;
    trackedObj.x = static_cast<int>(track.box.x);
    trackedObj.y = static_cast<int>(track.box.y);
    trackedObj.width = static_cast<int>(track.box.width);
    trackedObj.height = static_cast<int>(track.box.height);
    tracksOutput.push_back(trackedObj);
  }

  return tracksOutput;
}
