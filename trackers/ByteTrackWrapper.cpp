#include "ByteTrackWrapper.hpp"
#include "ByteTrack/Object.h"

ByteTrackWrapper::ByteTrackWrapper(const TrackConfig &config)
    : BaseTracker(config) {
  // Initialize ByteTrack with configuration from TrackConfig
  // Note: ByteTrack constructor is (frame_rate, track_buffer, track_thresh,
  // high_thresh, match_thresh) We'll use a default frame_rate of 30
  tracker_ = std::make_unique<byte_track::BYTETracker>(
      30, // frame_rate
      config.track_buffer, config.track_thresh, config.high_thresh,
      config.match_thresh);
}

ByteTrackWrapper::~ByteTrackWrapper() = default;

std::vector<TrackedObject>
ByteTrackWrapper::update(const std::vector<vision_core::Detection> &detections,
                         const cv::Mat &frame) {
  std::vector<TrackedObject> tracksOutput;
  std::vector<byte_track::Object> objects;

  // Filter and convert detections to ByteTrack format
  for (const auto &detection : detections) {
    if (config_.classes_to_track.find(static_cast<int>(detection.class_id)) !=
        config_.classes_to_track.end()) {
      byte_track::Rect<float> rect(detection.bbox.x, detection.bbox.y,
                                   detection.bbox.width, detection.bbox.height);
      byte_track::Object obj(rect, static_cast<int>(detection.class_id),
                             detection.class_confidence);
      objects.push_back(obj);
    }
  }

  // Update tracker
  std::vector<std::shared_ptr<byte_track::STrack>> tracks =
      tracker_->update(objects);

  // Convert tracks to TrackedObject format
  for (const auto &track : tracks) {
    TrackedObject trackedObj;
    trackedObj.track_id = static_cast<int>(track->getTrackId());
    const auto &rect = track->getRect();
    trackedObj.x = static_cast<int>(rect.x());
    trackedObj.y = static_cast<int>(rect.y());
    trackedObj.width = static_cast<int>(rect.width());
    trackedObj.height = static_cast<int>(rect.height());
    tracksOutput.push_back(trackedObj);
  }

  return tracksOutput;
}
