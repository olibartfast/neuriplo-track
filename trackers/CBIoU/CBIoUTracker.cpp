//
// CBIoUTracker.cpp: C-BIoU (Cascaded Buffered IoU) tracker implementation.
//
#include "CBIoUTracker.hpp"

#include "Hungarian.hpp"

#include <cmath>

namespace cbiou {
namespace {

bool isUsableBox(const cv::Rect_<float> &box) {
    return std::isfinite(box.x) && std::isfinite(box.y) && std::isfinite(box.width) && std::isfinite(box.height) &&
           box.width > 0.0f && box.height > 0.0f;
}

} // namespace

cv::Rect_<float> bufferBox(const cv::Rect_<float> &box, float scale) {
    const float dx = box.width * scale;
    const float dy = box.height * scale;
    return cv::Rect_<float>(box.x - dx, box.y - dy, box.width + 2 * dx, box.height + 2 * dy);
}

float bufferedIoU(const cv::Rect_<float> &a, const cv::Rect_<float> &b, float scale) {
    const cv::Rect_<float> buffered_a = bufferBox(a, scale);
    const cv::Rect_<float> buffered_b = bufferBox(b, scale);

    const float intersection = (buffered_a & buffered_b).area();
    const float uni = buffered_a.area() + buffered_b.area() - intersection;
    return uni > 1e-6f ? intersection / uni : 0.0f;
}

int CBIoUTrack::id_counter_ = 0;

void CBIoUTrack::resetIdCounter() { id_counter_ = 0; }

CBIoUTrack::CBIoUTrack(const cv::Rect_<float> &box, float det_score, int det_class_id, int motion_n)
    : score(det_score), class_id(det_class_id), motion_n_(motion_n > 0 ? static_cast<size_t>(motion_n) : 1) {
    id = id_counter_++;
    hits = 1;
    hit_streak = 1;
    observations_.push_back(box);
}

cv::Rect_<float> CBIoUTrack::predict() const {
    const cv::Rect_<float> &last = observations_.back();
    if (observations_.size() < 2) {
        return last;
    }

    // Mean per-frame displacement across the retained observations. No Kalman
    // filter, no acceleration term - the point of the paper is that a coarse
    // motion estimate plus a buffered matching space beats a confident but
    // wrong prediction.
    const cv::Rect_<float> &first = observations_.front();
    const float intervals = static_cast<float>(observations_.size() - 1);
    const float dx = (last.x - first.x) / intervals;
    const float dy = (last.y - first.y) / intervals;
    const float dw = (last.width - first.width) / intervals;
    const float dh = (last.height - first.height) / intervals;

    // While a track is unmatched the estimate keeps extending along the same
    // direction, one step per missed frame.
    const float steps = static_cast<float>(time_since_update + 1);
    cv::Rect_<float> predicted(last.x + dx * steps, last.y + dy * steps, last.width + dw * steps,
                               last.height + dh * steps);

    if (predicted.width <= 1.0f || predicted.height <= 1.0f) {
        predicted.width = last.width;
        predicted.height = last.height;
    }
    return predicted;
}

void CBIoUTrack::update(const cv::Rect_<float> &box, float det_score, int det_class_id) {
    observations_.push_back(box);
    while (observations_.size() > motion_n_ + 1) {
        observations_.pop_front();
    }

    score = det_score;
    class_id = det_class_id;
    hits += 1;
    hit_streak = time_since_update > 0 ? 1 : hit_streak + 1;
    time_since_update = 0;
}

void CBIoUTrack::markMissed() {
    if (time_since_update > 0) {
        hit_streak = 0;
    }
    time_since_update += 1;
}

CBIoUTracker::CBIoUTracker(int max_age, int min_hits, float iou_threshold, float b1, float b2, int motion_n)
    : max_age_(max_age), min_hits_(min_hits), iou_threshold_(iou_threshold), b1_(b1), b2_(b2), motion_n_(motion_n) {}

void CBIoUTracker::associate(const std::vector<DetectionBox> &detections,
                             const std::vector<cv::Rect_<float>> &predictions, float buffer,
                             std::vector<int> &detection_for_track, std::vector<bool> &detection_matched) const {
    std::vector<size_t> track_indices;
    for (size_t i = 0; i < predictions.size(); ++i) {
        if (detection_for_track[i] < 0) {
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
        for (size_t j = 0; j < detection_indices.size(); ++j) {
            iou_matrix[i][j] = bufferedIoU(predictions[track_indices[i]], detections[detection_indices[j]].box, buffer);
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

std::vector<TrackBox> CBIoUTracker::update(const std::vector<DetectionBox> &detections) {
    frame_count_ += 1;

    std::vector<DetectionBox> dets;
    dets.reserve(detections.size());
    for (const auto &detection : detections) {
        if (isUsableBox(detection.box)) {
            dets.push_back(detection);
        }
    }

    std::vector<cv::Rect_<float>> predictions;
    predictions.reserve(tracks_.size());
    for (const auto &track : tracks_) {
        predictions.push_back(track.predict());
    }

    std::vector<int> detection_for_track(tracks_.size(), -1);
    std::vector<bool> detection_matched(dets.size(), false);

    // Cascade: the tight buffer first, so confident pairs are settled before the
    // wider search radius gets a chance to steal them.
    associate(dets, predictions, b1_, detection_for_track, detection_matched);
    associate(dets, predictions, b2_, detection_for_track, detection_matched);

    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (detection_for_track[i] >= 0) {
            const DetectionBox &detection = dets[static_cast<size_t>(detection_for_track[i])];
            tracks_[i].update(detection.box, detection.score, detection.class_id);
        } else {
            tracks_[i].markMissed();
        }
    }

    for (size_t j = 0; j < dets.size(); ++j) {
        if (!detection_matched[j]) {
            tracks_.emplace_back(dets[j].box, dets[j].score, dets[j].class_id, motion_n_);
        }
    }

    std::vector<TrackBox> output;
    for (auto it = tracks_.begin(); it != tracks_.end();) {
        if (it->time_since_update < 1 && (it->hits >= min_hits_ || frame_count_ <= min_hits_)) {
            TrackBox track;
            track.id = it->id + 1;
            track.box = it->lastObservation();
            track.score = it->score;
            track.class_id = it->class_id;
            output.push_back(track);
        }

        if (it->time_since_update > max_age_) {
            it = tracks_.erase(it);
        } else {
            ++it;
        }
    }

    return output;
}

} // namespace cbiou
