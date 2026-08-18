//
// OCSort.cpp: OC-SORT (Observation-Centric SORT) class implementation.
//
#include "OCSort.hpp"

#include "Hungarian.hpp"

#include <cmath>

namespace ocsort {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float iou(const cv::Rect_<float> &a, const cv::Rect_<float> &b) {
    const float intersection = (a & b).area();
    const float uni = a.area() + b.area() - intersection;
    return uni > 1e-6f ? intersection / uni : 0.0f;
}

bool isUsableBox(const cv::Rect_<float> &box) {
    return std::isfinite(box.x) && std::isfinite(box.y) && std::isfinite(box.width) && std::isfinite(box.height) &&
           box.width > 0.0f && box.height > 0.0f;
}

cv::Point2f center(const cv::Rect_<float> &box) { return {box.x + box.width / 2, box.y + box.height / 2}; }

} // namespace

OCSort::OCSort(int max_age, int min_hits, float iou_threshold, int delta_t, float inertia, float det_thresh)
    : max_age_(max_age), min_hits_(min_hits), iou_threshold_(iou_threshold), delta_t_(delta_t > 0 ? delta_t : 1),
      inertia_(inertia), det_thresh_(det_thresh) {}

// OCM. Track i is paired with detection j by minimising
//   (1 - IoU) + inertia * (0.5 - direction_consistency) * detection_score
// where direction_consistency compares the track's direction of travel with the
// direction from its last observation to the candidate detection. It lies in
// [-0.5, 0.5], so a detection that continues the motion is favoured and one that
// contradicts it is penalised, without ever overriding IoU on its own.
void OCSort::associateWithMomentum(const std::vector<DetectionBox> &detections,
                                   const std::vector<cv::Rect_<float>> &predictions,
                                   std::vector<int> &detection_for_track, std::vector<bool> &detection_matched) const {
    const size_t track_num = predictions.size();
    const size_t detect_num = detections.size();
    if (track_num == 0 || detect_num == 0) {
        return;
    }

    std::vector<std::vector<double>> cost_matrix(track_num, std::vector<double>(detect_num, 0.0));
    std::vector<std::vector<float>> iou_matrix(track_num, std::vector<float>(detect_num, 0.0f));

    for (size_t i = 0; i < track_num; ++i) {
        const OCSortKalmanTracker &tracker = *trackers_[i];
        const cv::Point2f &velocity = tracker.velocityDirection();
        const bool has_direction = tracker.hasObservation() && (velocity.x != 0.0f || velocity.y != 0.0f);
        const cv::Point2f previous = center(tracker.lastObservation());

        for (size_t j = 0; j < detect_num; ++j) {
            iou_matrix[i][j] = iou(predictions[i], detections[j].box);

            float direction_cost = 0.0f;
            if (has_direction) {
                const cv::Point2f candidate = center(detections[j].box);
                const float dx = candidate.x - previous.x;
                const float dy = candidate.y - previous.y;
                const float norm = std::sqrt(dx * dx + dy * dy);
                if (norm > 1e-6f) {
                    float cos_angle = velocity.x * (dx / norm) + velocity.y * (dy / norm);
                    cos_angle = std::max(-1.0f, std::min(1.0f, cos_angle));
                    const float angle = std::acos(cos_angle);
                    direction_cost = (kPi / 2.0f - std::abs(angle)) / kPi;
                }
            }

            // The solver requires non-negative costs, so the reward form
            // (IoU + inertia * direction) is expressed as a distance. Since
            // direction_cost lies in [-0.5, 0.5], offsetting it by 0.5 keeps
            // every entry non-negative; the offset is constant per column and
            // therefore does not change the optimal assignment.
            cost_matrix[i][j] =
                static_cast<double>(1.0f - iou_matrix[i][j] + inertia_ * (0.5f - direction_cost) * detections[j].score);
        }
    }

    std::vector<int> assignment;
    HungarianAlgorithm solver;
    solver.Solve(cost_matrix, assignment);

    for (size_t i = 0; i < track_num; ++i) {
        const int detection_index = assignment[i];
        if (detection_index < 0) {
            continue;
        }
        // The assignment is optimal over the whole cost matrix, so a pair may
        // still be a poor match; IoU has the final say.
        if (iou_matrix[i][static_cast<size_t>(detection_index)] < iou_threshold_) {
            continue;
        }
        detection_for_track[i] = detection_index;
        detection_matched[static_cast<size_t>(detection_index)] = true;
    }
}

// OCR. Tracks that stayed unmatched are compared against the leftover
// detections using their last real observation rather than the drifting
// prediction, which is what lets a track survive a short disappearance.
void OCSort::associateWithObservations(const std::vector<DetectionBox> &detections,
                                       std::vector<int> &detection_for_track,
                                       std::vector<bool> &detection_matched) const {
    std::vector<size_t> track_indices;
    for (size_t i = 0; i < trackers_.size(); ++i) {
        if (detection_for_track[i] < 0 && trackers_[i]->hasObservation() &&
            isUsableBox(trackers_[i]->lastObservation())) {
            track_indices.push_back(i);
        }
    }

    std::vector<size_t> detection_indices;
    for (size_t j = 0; j < detections.size(); ++j) {
        if (!detection_matched[j]) {
            detection_indices.push_back(j);
        }
    }

    if (track_indices.empty() || detection_indices.empty()) {
        return;
    }

    std::vector<std::vector<double>> cost_matrix(track_indices.size(),
                                                 std::vector<double>(detection_indices.size(), 0.0));
    std::vector<std::vector<float>> iou_matrix(track_indices.size(),
                                               std::vector<float>(detection_indices.size(), 0.0f));

    for (size_t i = 0; i < track_indices.size(); ++i) {
        const cv::Rect_<float> &observation = trackers_[track_indices[i]]->lastObservation();
        for (size_t j = 0; j < detection_indices.size(); ++j) {
            iou_matrix[i][j] = iou(observation, detections[detection_indices[j]].box);
            cost_matrix[i][j] = 1.0 - static_cast<double>(iou_matrix[i][j]);
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
        const size_t detection_index = detection_indices[static_cast<size_t>(column)];
        detection_for_track[track_indices[i]] = static_cast<int>(detection_index);
        detection_matched[detection_index] = true;
    }
}

std::vector<TrackBox> OCSort::update(const std::vector<DetectionBox> &detections) {
    frame_count_ += 1;

    // OC-SORT associates high-confidence detections only; the rest are dropped
    // rather than used to keep a track alive.
    std::vector<DetectionBox> dets;
    dets.reserve(detections.size());
    for (const auto &detection : detections) {
        if (detection.score >= det_thresh_ && isUsableBox(detection.box)) {
            dets.push_back(detection);
        }
    }

    // Predict, dropping trackers whose state has become unusable.
    std::vector<cv::Rect_<float>> predictions;
    predictions.reserve(trackers_.size());
    for (auto it = trackers_.begin(); it != trackers_.end();) {
        const cv::Rect_<float> predicted = (*it)->predict();
        if (isUsableBox(predicted)) {
            predictions.push_back(predicted);
            ++it;
        } else {
            it = trackers_.erase(it);
        }
    }

    std::vector<int> detection_for_track(trackers_.size(), -1);
    std::vector<bool> detection_matched(dets.size(), false);

    associateWithMomentum(dets, predictions, detection_for_track, detection_matched);
    associateWithObservations(dets, detection_for_track, detection_matched);

    for (size_t i = 0; i < trackers_.size(); ++i) {
        if (detection_for_track[i] >= 0) {
            const DetectionBox &detection = dets[static_cast<size_t>(detection_for_track[i])];
            trackers_[i]->update(detection.box, detection.score);
            trackers_[i]->class_id = detection.class_id;
        } else {
            trackers_[i]->markMissed();
        }
    }

    for (size_t j = 0; j < dets.size(); ++j) {
        if (!detection_matched[j]) {
            trackers_.push_back(
                std::make_unique<OCSortKalmanTracker>(dets[j].box, dets[j].score, dets[j].class_id, delta_t_));
        }
    }

    std::vector<TrackBox> output;
    for (auto it = trackers_.begin(); it != trackers_.end();) {
        OCSortKalmanTracker &tracker = **it;
        if (tracker.time_since_update < 1 && (tracker.hit_streak >= min_hits_ || frame_count_ <= min_hits_)) {
            TrackBox track;
            track.id = tracker.id + 1;
            track.box = tracker.outputBox();
            track.score = tracker.score;
            track.class_id = tracker.class_id;
            output.push_back(track);
        }

        if (tracker.time_since_update > max_age_) {
            it = trackers_.erase(it);
        } else {
            ++it;
        }
    }

    return output;
}

} // namespace ocsort
