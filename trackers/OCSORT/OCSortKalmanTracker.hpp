//
// OCSortKalmanTracker.hpp: observation-centric Kalman box tracker used by OC-SORT.
//
// Extends the SORT box tracker with the state OC-SORT needs
// (https://arxiv.org/abs/2203.14360):
//   - the history of real observations, keyed by track age,
//   - the last observation, used by observation-centric recovery (OCR),
//   - a velocity direction estimated from observations delta_t frames apart,
//     used by observation-centric momentum (OCM),
//   - an observation-centric re-update (ORU) that replays the Kalman filter over
//     a virtual trajectory when a track is re-associated after a gap.
//
#pragma once
#include <map>
#include <opencv2/core/types.hpp>
#include <opencv2/video/tracking.hpp>

namespace ocsort {

class OCSortKalmanTracker {
  public:
    OCSortKalmanTracker(const cv::Rect_<float> &box, float score, int class_id, int delta_t);

    // Advances the filter by one frame and returns the predicted box.
    cv::Rect_<float> predict();

    // Associates a real observation: estimates the OCM direction, replays the
    // filter over the virtual trajectory covering any gap (ORU), then corrects.
    void update(const cv::Rect_<float> &box, float score);

    // Marks a frame without an associated observation.
    void markMissed();

    cv::Rect_<float> state() const;

    // Box reported to the caller: the last real observation when there is one,
    // the filter state otherwise.
    cv::Rect_<float> outputBox() const;

    bool hasObservation() const { return has_observation_; }
    const cv::Rect_<float> &lastObservation() const { return last_observation_; }

    // Unit-length direction of travel, or (0, 0) when it is not known yet.
    const cv::Point2f &velocityDirection() const { return velocity_; }

    static void resetIdCounter();

    int id{};
    int time_since_update{};
    int hits{};
    int hit_streak{};
    int age{};
    float score{};
    int class_id{};

  private:
    void initKalmanFilter(const cv::Rect_<float> &box);
    void correct(const cv::Rect_<float> &box);
    cv::Rect_<float> boxFromState(float cx, float cy, float s, float r) const;

    static int id_counter_;

    cv::KalmanFilter kf_;
    cv::Mat measurement_;

    std::map<int, cv::Rect_<float>> observations_;
    cv::Rect_<float> last_observation_{};
    bool has_observation_{false};
    cv::Point2f velocity_{0.0f, 0.0f};
    int delta_t_{3};
};

} // namespace ocsort
