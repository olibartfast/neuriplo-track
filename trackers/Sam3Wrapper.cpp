#include "Sam3Wrapper.hpp"

#include <neuriplo/tasks/core/opencv_interop.hpp>
#include <stdexcept>

Sam3Wrapper::Sam3Wrapper(const TrackConfig &config, std::unique_ptr<tracking::MaskPropagator> propagator)
    : BaseTracker(config),
      tracker_(std::make_unique<sam3::Sam3Tracker>(
          config.max_age, config.min_hits, config.iou_threshold, config.sam_presence_thresh, config.sam_det_thresh,
          config.sam_reprompt_iou, config.sam_max_objects, config.mask_iou_weight, std::move(propagator))) {}

Sam3Wrapper::~Sam3Wrapper() = default;

namespace {

void requireFrame(const cv::Mat &frame) {
    if (frame.empty()) {
        throw std::invalid_argument(
            "Sam3Wrapper::update needs the frame pixels: SAM3 propagates masks, it does not predict boxes");
    }
}

sam3::DetectionBox toDetectionBox(const neuriplo_tasks::Detection &detection) {
    sam3::DetectionBox box;
    box.box = cv::Rect_<float>(neuriplo_tasks::toCvRect(detection.bbox));
    box.score = detection.class_confidence;
    box.class_id = static_cast<int>(detection.class_id);
    return box;
}

bool isTrackedClass(const TrackConfig &config, int class_id) {
    return config.classes_to_track.find(class_id) != config.classes_to_track.end();
}

std::vector<TrackedObject> toTrackedObjects(const std::vector<sam3::TrackBox> &tracks) {
    std::vector<TrackedObject> tracks_output;
    tracks_output.reserve(tracks.size());
    for (const auto &track : tracks) {
        TrackedObject tracked_obj;
        tracked_obj.track_id = track.id;
        tracked_obj.x = track.box.x;
        tracked_obj.y = track.box.y;
        tracked_obj.width = track.box.width;
        tracked_obj.height = track.box.height;
        tracked_obj.confidence = track.score;
        tracks_output.push_back(tracked_obj);
    }
    return tracks_output;
}

} // namespace

std::vector<TrackedObject> Sam3Wrapper::update(const std::vector<neuriplo_tasks::Detection> &detections,
                                               const cv::Mat &frame) {
    requireFrame(frame);

    std::vector<sam3::DetectionBox> detections_to_track;
    detections_to_track.reserve(detections.size());
    for (const auto &detection : detections) {
        if (isTrackedClass(config_, static_cast<int>(detection.class_id))) {
            detections_to_track.push_back(toDetectionBox(detection));
        }
    }

    return toTrackedObjects(tracker_->update(detections_to_track, frame));
}

std::vector<TrackedObject> Sam3Wrapper::update(const std::vector<neuriplo_tasks::InstanceSegmentation> &segmentations,
                                               const cv::Mat &frame) {
    requireFrame(frame);

    std::vector<sam3::DetectionBox> detections_to_track;
    detections_to_track.reserve(segmentations.size());
    for (const auto &segmentation : segmentations) {
        if (isTrackedClass(config_, static_cast<int>(segmentation.class_id))) {
            sam3::DetectionBox box = toDetectionBox(segmentation);
            // Only pay for the crop when the mask will actually be consulted.
            if (config_.mask_iou_weight > 0.0f) {
                box.mask = tracking::maskRegionFrom(segmentation);
            }
            detections_to_track.push_back(std::move(box));
        }
    }

    return toTrackedObjects(tracker_->update(detections_to_track, frame));
}
