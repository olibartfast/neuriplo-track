#include "OCSortWrapper.hpp"

#include <neuriplo/tasks/core/opencv_interop.hpp>

OCSortWrapper::OCSortWrapper(const TrackConfig &config)
    : BaseTracker(config), tracker_(std::make_unique<ocsort::OCSort>(config.max_age, config.min_hits,
                                                                     config.iou_threshold, config.ocsort_delta_t,
                                                                     config.ocsort_inertia, config.ocsort_det_thresh)) {
}

OCSortWrapper::~OCSortWrapper() = default;

std::vector<TrackedObject> OCSortWrapper::update(const std::vector<neuriplo_tasks::Detection> &detections,
                                                 const cv::Mat &frame) {
    static_cast<void>(frame);

    std::vector<ocsort::DetectionBox> detectionsToTrack;
    detectionsToTrack.reserve(detections.size());

    // Filter detections based on class IDs and convert to the tracker's box type
    for (const auto &detection : detections) {
        if (config_.classes_to_track.find(static_cast<int>(detection.class_id)) != config_.classes_to_track.end()) {
            ocsort::DetectionBox box;
            box.box = cv::Rect_<float>(neuriplo_tasks::toCvRect(detection.bbox));
            box.score = detection.class_confidence;
            box.class_id = static_cast<int>(detection.class_id);
            detectionsToTrack.push_back(box);
        }
    }

    const std::vector<ocsort::TrackBox> tracks = tracker_->update(detectionsToTrack);

    std::vector<TrackedObject> tracksOutput;
    tracksOutput.reserve(tracks.size());
    for (const auto &track : tracks) {
        TrackedObject trackedObj;
        trackedObj.track_id = track.id;
        trackedObj.x = track.box.x;
        trackedObj.y = track.box.y;
        trackedObj.width = track.box.width;
        trackedObj.height = track.box.height;
        trackedObj.confidence = track.score;
        tracksOutput.push_back(trackedObj);
    }

    return tracksOutput;
}
