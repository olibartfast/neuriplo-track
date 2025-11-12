#include "SortWrapper.hpp"

SortWrapper::SortWrapper(const TrackConfig& config)
    : BaseTracker(config), tracker_(std::make_unique<Sort>(config.max_age, config.min_hits, config.iou_threshold))
{
}

SortWrapper::~SortWrapper() = default;

std::vector<TrackedObject> SortWrapper::update(const std::vector<Detection>& detections, const cv::Mat& frame)
{
    std::vector<TrackedObject> tracksOutput;
    std::vector<cv::Rect> detectionsToTrack;

    // Filter detections based on class IDs
    for (const auto& detection : detections) {
        if (config_.classes_to_track.find(detection.label) != config_.classes_to_track.end()) {
            detectionsToTrack.push_back(detection.bbox);
        }
    }

    // Update tracker
    std::vector<Track> tracks = tracker_->update(detectionsToTrack);

    // Convert tracks to TrackedObject format
    for (const auto& track : tracks) {
        TrackedObject trackedObj;
        trackedObj.track_id = track.id;
        trackedObj.x = static_cast<int>(track.bbox.x);
        trackedObj.y = static_cast<int>(track.bbox.y);
        trackedObj.width = static_cast<int>(track.bbox.width);
        trackedObj.height = static_cast<int>(track.bbox.height);
        tracksOutput.push_back(trackedObj);
    }

    return tracksOutput;
}
