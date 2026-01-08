#include "BoTSORTWrapper.hpp"

BoTSORTWrapper::BoTSORTWrapper(const TrackConfig &config)
    : BaseTracker(config) {
  // Initialize BoTSORT with configuration paths
  tracker_ = std::make_unique<botsort::BoTSORT>(
      config.tracker_config_path, config.gmc_config_path,
      config.reid_config_path, config.reid_onnx_path);
}

BoTSORTWrapper::~BoTSORTWrapper() = default;

std::vector<TrackedObject>
BoTSORTWrapper::update(const std::vector<vision_core::Detection> &detections,
                       const cv::Mat &frame) {
  std::vector<TrackedObject> tracksOutput;
  std::vector<botsort::Detection> detectionsToTrack;

  // Filter and convert detections
  for (const auto &detection : detections) {
    if (config_.classes_to_track.find(static_cast<int>(detection.class_id)) !=
        config_.classes_to_track.end()) {
      botsort::Detection detBox;
      detBox.bbox_tlwh = cv::Rect_<float>(detection.bbox);
      detBox.confidence = detection.class_confidence;
      detBox.class_id = static_cast<int>(detection.class_id);
      detectionsToTrack.push_back(detBox);
    }
  }

  // Update tracker
  std::vector<std::shared_ptr<botsort::Track>> tracks =
      tracker_->track(detectionsToTrack, frame);

  // Convert tracks to TrackedObject format
  for (const auto &track : tracks) {
    TrackedObject trackedObj;
    trackedObj.track_id = track->track_id;
    auto tlwh = track->get_tlwh();
    trackedObj.x = static_cast<int>(tlwh[0]);
    trackedObj.y = static_cast<int>(tlwh[1]);
    trackedObj.width = static_cast<int>(tlwh[2]);
    trackedObj.height = static_cast<int>(tlwh[3]);
    tracksOutput.push_back(trackedObj);
  }

  return tracksOutput;
}
