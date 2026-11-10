//
// Sam3Tracker.cpp: detector-prompted tracker loop over a MaskPropagator.
//
#include "Sam3Tracker.hpp"

#include "Hungarian.hpp"

#include <cmath>
#include <glog/logging.h>
#include <stdexcept>
#include <unordered_map>

namespace sam3 {
namespace {

// See the note in OCSort.cpp: an invalid edge must not be able to displace a
// valid pairing just because the solver has to assign min(rows, cols) pairs.
constexpr double kUnmatchableCost = 1e6;

float boxIoU(const cv::Rect_<float> &a, const cv::Rect_<float> &b) {
    const float intersection = (a & b).area();
    const float uni = a.area() + b.area() - intersection;
    return uni > 1e-6f ? intersection / uni : 0.0f;
}

bool isUsableBox(const cv::Rect_<float> &box) {
    return std::isfinite(box.x) && std::isfinite(box.y) && std::isfinite(box.width) && std::isfinite(box.height) &&
           box.width > 0.0f && box.height > 0.0f;
}

} // namespace

Sam3Tracker::Sam3Tracker(int max_age, int min_hits, float iou_threshold, float presence_thresh, float det_thresh,
                         float reprompt_iou, int max_objects, float mask_iou_weight,
                         std::unique_ptr<tracking::MaskPropagator> propagator)
    : max_age_(max_age), min_hits_(min_hits), iou_threshold_(iou_threshold), presence_thresh_(presence_thresh),
      det_thresh_(det_thresh), reprompt_iou_(reprompt_iou), max_objects_(max_objects),
      mask_iou_weight_(mask_iou_weight), propagator_(std::move(propagator)) {
    if (!propagator_) {
        throw std::invalid_argument("Sam3Tracker needs a MaskPropagator: inference lives upstream, not in trackers/");
    }
}

int Sam3Tracker::effectiveCapacity() const {
    const int propagator_capacity = propagator_->capacity();
    if (max_objects_ <= 0) {
        return propagator_capacity;
    }
    return std::min(max_objects_, propagator_capacity);
}

std::vector<TrackBox> Sam3Tracker::update(const std::vector<DetectionBox> &detections, const cv::Mat &frame) {
    // 1. Propagate every live object onto this frame.
    const std::vector<tracking::PropagatedMask> propagated = propagator_->propagate(frame);
    std::unordered_map<int, const tracking::PropagatedMask *> by_id;
    by_id.reserve(propagated.size());
    for (const auto &entry : propagated) {
        by_id[entry.id] = &entry;
    }

    for (auto &track : tracks_) {
        const auto found = by_id.find(track.prop_id);
        // An empty mask with high presence is contradictory; treat it as
        // absent rather than reporting a degenerate box.
        track.present =
            found != by_id.end() && found->second->presence >= presence_thresh_ && !found->second->mask.empty();
        track.mask = found != by_id.end() ? found->second->mask : tracking::MaskRegion{};
    }

    // 2. Score-gate and validate the detections. Class filtering happens in
    // the wrapper, as for every other tracker.
    std::vector<DetectionBox> dets;
    dets.reserve(detections.size());
    for (const auto &detection : detections) {
        if (detection.score >= det_thresh_ && isUsableBox(detection.box)) {
            dets.push_back(detection);
        }
    }

    // 3. Associate the present objects to the detections. Absent objects sit
    // this round out: they age below, and matching resumes when they return.
    std::vector<size_t> track_indices;
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].present) {
            track_indices.push_back(i);
        }
    }

    std::vector<int> detection_for_track(tracks_.size(), -1);
    std::vector<bool> detection_matched(dets.size(), false);

    if (!track_indices.empty() && !dets.empty()) {
        std::vector<std::vector<double>> cost_matrix(track_indices.size(), std::vector<double>(dets.size(), 0.0));
        std::vector<std::vector<float>> iou_matrix(track_indices.size(), std::vector<float>(dets.size(), 0.0f));

        for (size_t i = 0; i < track_indices.size(); ++i) {
            const Track &track = tracks_[track_indices[i]];
            const cv::Rect_<float> track_box(track.mask.bounds());
            for (size_t j = 0; j < dets.size(); ++j) {
                // The mask cue rides on top of the box overlap between the
                // propagated mask's box and the detection box, so a pair where
                // either side carries no mask associates on boxes alone.
                iou_matrix[i][j] =
                    tracking::blendOverlap(boxIoU(track_box, dets[j].box), track.mask, dets[j].mask, mask_iou_weight_);
                cost_matrix[i][j] =
                    iou_matrix[i][j] < iou_threshold_ ? kUnmatchableCost : 1.0 - static_cast<double>(iou_matrix[i][j]);
            }
        }

        std::vector<int> assignment;
        HungarianAlgorithm solver;
        solver.Solve(cost_matrix, assignment);

        for (size_t i = 0; i < track_indices.size(); ++i) {
            const int column = assignment[i];
            if (column < 0 || iou_matrix[i][static_cast<size_t>(column)] < iou_threshold_) {
                continue;
            }
            detection_for_track[track_indices[i]] = column;
            detection_matched[static_cast<size_t>(column)] = true;
        }
    }

    // 4. Apply the matches. A matched pair whose propagated box has drifted
    // from its detection is re-prompted with the detection box, keeping the id
    // and the memory instead of restarting the object.
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (detection_for_track[i] < 0) {
            continue;
        }
        Track &track = tracks_[i];
        const DetectionBox &detection = dets[static_cast<size_t>(detection_for_track[i])];
        const cv::Rect_<float> propagated_box(track.mask.bounds());
        if (boxIoU(propagated_box, detection.box) < reprompt_iou_) {
            propagator_->reprompt(track.prop_id, detection.box, frame);
        }
        track.hits += 1;
        track.missed = 0;
        track.score = detection.score;
        track.class_id = detection.class_id;
    }

    // 5. Age the unmatched and drop the lost. Removal is driven by detector
    // support, not by presence: a masklet SAM still reports but no detection
    // backs for more than max_age frames is removed all the same. This runs
    // before births so newborns skip aging on the frame they arrive.
    for (auto it = tracks_.begin(); it != tracks_.end();) {
        if (detection_for_track[static_cast<size_t>(it - tracks_.begin())] < 0) {
            it->missed += 1;
        }
        if (it->missed > max_age_) {
            propagator_->removeObject(it->prop_id);
            it = tracks_.erase(it);
        } else {
            ++it;
        }
    }

    // 6. Unmatched detections birth new objects while there is capacity.
    for (size_t j = 0; j < dets.size(); ++j) {
        if (detection_matched[j]) {
            continue;
        }
        if (static_cast<int>(tracks_.size()) >= effectiveCapacity()) {
            if (!capacity_warned_) {
                capacity_warned_ = true;
                LOG(WARNING) << "Sam3Tracker at capacity (" << effectiveCapacity()
                             << " live objects): further detections are untracked until an object is removed";
            }
            continue;
        }
        Track track;
        track.prop_id = propagator_->addObject(dets[j].box, frame);
        track.hits = 1;
        track.score = dets[j].score;
        track.class_id = dets[j].class_id;
        // Presence and mask arrive with the next propagate() call.
        tracks_.push_back(track);
    }

    // 7. Report one box per present object with enough detection matches, box
    // taken from the propagated mask, confidence from the last match.
    std::vector<TrackBox> output;
    for (const auto &track : tracks_) {
        if (!track.present || track.hits < min_hits_) {
            continue;
        }
        TrackBox track_box;
        track_box.id = track.prop_id + 1;
        const cv::Rect bounds = track.mask.bounds();
        track_box.box = cv::Rect_<float>(static_cast<float>(bounds.x), static_cast<float>(bounds.y),
                                         static_cast<float>(bounds.width), static_cast<float>(bounds.height));
        track_box.score = track.score;
        track_box.class_id = track.class_id;
        output.push_back(track_box);
    }

    return output;
}

} // namespace sam3
