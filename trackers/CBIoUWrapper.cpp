#include "CBIoUWrapper.hpp"

#include <neuriplo/tasks/core/opencv_interop.hpp>

CBIoUWrapper::CBIoUWrapper(const TrackConfig &config)
    : BaseTracker(config),
      tracker_(std::make_unique<cbiou::CBIoUTracker>(config.max_age, config.min_hits, config.iou_threshold,
                                                     config.cbiou_b1, config.cbiou_b2, config.cbiou_motion_n)) {}

CBIoUWrapper::~CBIoUWrapper() = default;

std::vector<TrackedObject> CBIoUWrapper::update(const std::vector<neuriplo_tasks::Detection> &detections,
                                                const cv::Mat &frame) {
    static_cast<void>(frame);

    std::vector<cbiou::DetectionBox> detectionsToTrack;
    detectionsToTrack.reserve(detections.size());

    // Filter detections based on class IDs and convert to the tracker's box type
    for (const auto &detection : detections) {
        if (config_.classes_to_track.find(static_cast<int>(detection.class_id)) != config_.classes_to_track.end()) {
            cbiou::DetectionBox box;
            box.box = cv::Rect_<float>(neuriplo_tasks::toCvRect(detection.bbox));
            box.score = detection.class_confidence;
            box.class_id = static_cast<int>(detection.class_id);
            detectionsToTrack.push_back(box);
        }
    }

    const std::vector<cbiou::TrackBox> tracks = tracker_->update(detectionsToTrack);

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
