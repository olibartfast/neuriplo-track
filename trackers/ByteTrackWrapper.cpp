#include "ByteTrackWrapper.hpp"

ByteTrackWrapper::ByteTrackWrapper(const TrackConfig& config)
    : BaseTracker(config)
{
    // Initialize ByteTrack with configuration from TrackConfig
    tracker_ = std::make_unique<BYTETracker>(
        config.track_buffer,
        config.track_thresh,
        config.high_thresh,
        config.match_thresh
    );
}

ByteTrackWrapper::~ByteTrackWrapper() = default;

std::vector<TrackedObject> ByteTrackWrapper::update(const std::vector<Detection>& detections, const cv::Mat& frame)
{
    std::vector<TrackedObject> tracksOutput;
    std::vector<Object> objects;

    // Filter and convert detections to ByteTrack format
    for (const auto& detection : detections) {
        if (config_.classes_to_track.find(detection.label) != config_.classes_to_track.end()) {
            Object obj;
            obj.rect.x = detection.bbox.x;
            obj.rect.y = detection.bbox.y;
            obj.rect.width = detection.bbox.width;
            obj.rect.height = detection.bbox.height;
            obj.prob = detection.score;
            obj.label = detection.label;
            objects.push_back(obj);
        }
    }

    // Update tracker
    std::vector<STrack> tracks = tracker_->update(objects);

    // Convert tracks to TrackedObject format
    for (const auto& track : tracks) {
        TrackedObject trackedObj;
        trackedObj.track_id = track.track_id;
        trackedObj.x = static_cast<int>(track.tlwh[0]);
        trackedObj.y = static_cast<int>(track.tlwh[1]);
        trackedObj.width = static_cast<int>(track.tlwh[2]);
        trackedObj.height = static_cast<int>(track.tlwh[3]);
        tracksOutput.push_back(trackedObj);
    }

    return tracksOutput;
}
