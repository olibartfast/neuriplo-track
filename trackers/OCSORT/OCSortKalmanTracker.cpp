//
// OCSortKalmanTracker.cpp: observation-centric Kalman box tracker implementation.
//
#include "OCSortKalmanTracker.hpp"

#include <cmath>

namespace ocsort {

int OCSortKalmanTracker::id_counter_ = 0;

void OCSortKalmanTracker::resetIdCounter() { id_counter_ = 0; }

OCSortKalmanTracker::OCSortKalmanTracker(const cv::Rect_<float> &box, float det_score, int det_class_id, int delta_t)
    : score(det_score), class_id(det_class_id), delta_t_(delta_t > 0 ? delta_t : 1) {
    initKalmanFilter(box);

    id = id_counter_++;
    last_observation_ = box;
    has_observation_ = true;
    observations_[age] = box;
}

// Same constant-velocity model as SORT: state is [cx, cy, s, r, dcx, dcy, ds].
void OCSortKalmanTracker::initKalmanFilter(const cv::Rect_<float> &box) {
    constexpr int stateNum = 7;
    constexpr int measureNum = 4;
    kf_ = cv::KalmanFilter(stateNum, measureNum, 0);

    measurement_ = cv::Mat::zeros(measureNum, 1, CV_32F);

    kf_.transitionMatrix =
        (cv::Mat_<float>(stateNum, stateNum) << 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
         1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);

    cv::setIdentity(kf_.measurementMatrix);
    cv::setIdentity(kf_.processNoiseCov, cv::Scalar::all(1e-2));
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(1));

    kf_.statePost.at<float>(0, 0) = box.x + box.width / 2;
    kf_.statePost.at<float>(1, 0) = box.y + box.height / 2;
    kf_.statePost.at<float>(2, 0) = box.area();
    kf_.statePost.at<float>(3, 0) = box.height > 0.0f ? box.width / box.height : 1.0f;
}

cv::Rect_<float> OCSortKalmanTracker::predict() {
    const cv::Mat p = kf_.predict();
    age += 1;

    if (time_since_update > 0) {
        hit_streak = 0;
    }
    time_since_update += 1;

    return boxFromState(p.at<float>(0, 0), p.at<float>(1, 0), p.at<float>(2, 0), p.at<float>(3, 0));
}

void OCSortKalmanTracker::markMissed() {
    // Nothing to correct; predict() has already aged the track.
}

void OCSortKalmanTracker::correct(const cv::Rect_<float> &box) {
    measurement_.at<float>(0, 0) = box.x + box.width / 2;
    measurement_.at<float>(1, 0) = box.y + box.height / 2;
    measurement_.at<float>(2, 0) = box.area();
    measurement_.at<float>(3, 0) = box.height > 0.0f ? box.width / box.height : 1.0f;

    kf_.correct(measurement_);
}

void OCSortKalmanTracker::update(const cv::Rect_<float> &box, float det_score) {
    if (has_observation_) {
        // OCM: estimate the direction of travel from an observation delta_t
        // frames back, falling back to the most recent one. Comparing distant
        // observations makes the direction far less noise-sensitive than
        // comparing consecutive frames.
        const cv::Rect_<float> *previous = &last_observation_;
        for (int i = 0; i < delta_t_; ++i) {
            const auto found = observations_.find(age - (delta_t_ - i));
            if (found != observations_.end()) {
                previous = &found->second;
                break;
            }
        }

        const float cx1 = previous->x + previous->width / 2;
        const float cy1 = previous->y + previous->height / 2;
        const float cx2 = box.x + box.width / 2;
        const float cy2 = box.y + box.height / 2;
        const float dx = cx2 - cx1;
        const float dy = cy2 - cy1;
        const float norm = std::sqrt(dx * dx + dy * dy);
        velocity_ = norm > 1e-6f ? cv::Point2f(dx / norm, dy / norm) : cv::Point2f(0.0f, 0.0f);

        // ORU: the track just came back after a gap, so the filter has been
        // running on predictions only. Replay it over a virtual trajectory
        // interpolated between the last real observation and this one, which
        // keeps the accumulated prediction error from being baked into the state.
        if (time_since_update > 1) {
            const int steps = time_since_update;
            for (int i = 1; i < steps; ++i) {
                const float alpha = static_cast<float>(i) / static_cast<float>(steps);
                const cv::Rect_<float> virtual_box(
                    last_observation_.x + alpha * (box.x - last_observation_.x),
                    last_observation_.y + alpha * (box.y - last_observation_.y),
                    last_observation_.width + alpha * (box.width - last_observation_.width),
                    last_observation_.height + alpha * (box.height - last_observation_.height));
                correct(virtual_box);
                kf_.predict();
            }
        }
    }

    last_observation_ = box;
    has_observation_ = true;
    observations_[age] = box;
    score = det_score;

    time_since_update = 0;
    hits += 1;
    hit_streak += 1;

    correct(box);
}

cv::Rect_<float> OCSortKalmanTracker::state() const {
    const cv::Mat s = kf_.statePost;
    return boxFromState(s.at<float>(0, 0), s.at<float>(1, 0), s.at<float>(2, 0), s.at<float>(3, 0));
}

cv::Rect_<float> OCSortKalmanTracker::outputBox() const { return has_observation_ ? last_observation_ : state(); }

// Convert [cx, cy, s, r] back to [x, y, w, h].
cv::Rect_<float> OCSortKalmanTracker::boxFromState(float cx, float cy, float s, float r) const {
    if (s <= 0.0f || r <= 0.0f) {
        return cv::Rect_<float>(0.0f, 0.0f, 0.0f, 0.0f);
    }

    const float w = std::sqrt(s * r);
    const float h = w > 0.0f ? s / w : 0.0f;
    float x = cx - w / 2;
    float y = cy - h / 2;

    if (x < 0 && cx > 0) {
        x = 0;
    }
    if (y < 0 && cy > 0) {
        y = 0;
    }

    return cv::Rect_<float>(x, y, w, h);
}

} // namespace ocsort
