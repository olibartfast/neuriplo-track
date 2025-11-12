#include "BoTSORTWrapper.hpp"

BoTSORTWrapper::BoTSORTWrapper(const TrackConfig& config)
    : BaseTracker(config)
{
    // Initialize BoTSORT with configuration paths
    tracker_ = std::make_unique<BoTSORT>(
        config.tracker_config_path,
        config.gmc_config_path,
        config.reid_config_path,
        config.reid_onnx_path
    );
}

BoTSORTWrapper::~BoTSORTWrapper() = default;

std::vector<TrackedObject> BoTSORTWrapper::update(const std::vector<Detection>& detections, const cv::Mat& frame)
{
    std::vector<TrackedObject> tracksOutput;
    std::vector<DetectionBox> detectionsToTrack;

    // Filter and convert detections
    for (const auto& detection : detections) {
        if (config_.classes_to_track.find(detection.label) != config_.classes_to_track.end()) {
            DetectionBox detBox;
            detBox.tlwh = {
                static_cast<float>(detection.bbox.x),
                static_cast<float>(detection.bbox.y),
                static_cast<float>(detection.bbox.width),
                static_cast<float>(detection.bbox.height)
            };
            detBox.confidence = detection.score;
            detBox.classId = detection.label;
            detectionsToTrack.push_back(detBox);
        }
    }

    // Update tracker
    std::vector<STrackBoTSORT> tracks = tracker_->update(detectionsToTrack, frame);

    // Convert tracks to TrackedObject format
    for (const auto& track : tracks) {
        TrackedObject trackedObj;
        trackedObj.track_id = track.trackId;
        trackedObj.x = static_cast<int>(track.tlwh[0]);
        trackedObj.y = static_cast<int>(track.tlwh[1]);
        trackedObj.width = static_cast<int>(track.tlwh[2]);
        trackedObj.height = static_cast<int>(track.tlwh[3]);
        tracksOutput.push_back(trackedObj);
    }

    return tracksOutput;
}
