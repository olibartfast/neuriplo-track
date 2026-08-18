#include "OCSortWrapper.hpp"

#include <neuriplo/tasks/core/opencv_interop.hpp>

OCSortWrapper::OCSortWrapper(const TrackConfig &config)
    : BaseTracker(config),
      tracker_(std::make_unique<ocsort::OCSort>(config.max_age, config.min_hits, config.iou_threshold,
                                                config.ocsort_delta_t, config.ocsort_inertia, config.ocsort_det_thresh,
                                                config.mask_iou_weight)) {}

OCSortWrapper::~OCSortWrapper() = default;

namespace {

ocsort::DetectionBox toDetectionBox(const neuriplo_tasks::Detection &detection) {
    ocsort::DetectionBox box;
    box.box = cv::Rect_<float>(neuriplo_tasks::toCvRect(detection.bbox));
    box.score = detection.class_confidence;
    box.class_id = static_cast<int>(detection.class_id);
    return box;
}

} // namespace

std::vector<TrackedObject> OCSortWrapper::update(const std::vector<neuriplo_tasks::Detection> &detections,
                                                 const cv::Mat &frame) {
    static_cast<void>(frame);

    std::vector<ocsort::DetectionBox> detectionsToTrack;
    detectionsToTrack.reserve(detections.size());

    // Filter detections based on class IDs and convert to the tracker's box type
    for (const auto &detection : detections) {
        if (config_.classes_to_track.find(static_cast<int>(detection.class_id)) != config_.classes_to_track.end()) {
            detectionsToTrack.push_back(toDetectionBox(detection));
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

std::vector<TrackedObject> OCSortWrapper::update(const std::vector<neuriplo_tasks::InstanceSegmentation> &segmentations,
                                                 const cv::Mat &frame) {
    static_cast<void>(frame);

    std::vector<ocsort::DetectionBox> detectionsToTrack;
    detectionsToTrack.reserve(segmentations.size());

    for (const auto &segmentation : segmentations) {
        if (config_.classes_to_track.find(static_cast<int>(segmentation.class_id)) != config_.classes_to_track.end()) {
            ocsort::DetectionBox box = toDetectionBox(segmentation);
            // Only pay for the crop when the mask will actually be consulted.
            if (config_.mask_iou_weight > 0.0f) {
                box.mask = tracking::maskRegionFrom(segmentation);
            }
            detectionsToTrack.push_back(std::move(box));
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
