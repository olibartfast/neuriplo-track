//
// CBIoUTracker.hpp: C-BIoU (Cascaded Buffered IoU) tracker class declaration.
//
// Yang et al., "Hard to Track Objects with Irregular Motions and Similar
// Appearances? Make It Easier by Buffering the Matching Space"
// (https://arxiv.org/abs/2211.14317).
//
// The observation behind the paper is that a Kalman filter with a
// constant-velocity model is the wrong tool for erratic motion: it predicts
// confidently in the wrong direction. C-BIoU replaces it with two much simpler
// ideas:
//   - Buffered IoU: enlarge both the track box and the detection box by a scale
//     factor before computing IoU, so boxes that no longer overlap can still be
//     matched. The buffer is the matching search radius.
//   - Cascaded matching: match at a small buffer first, so easy pairs are taken
//     by their nearest match, then retry the leftovers at a larger buffer.
// Motion is estimated as the mean displacement over the last n observations.
//
#pragma once
#include <deque>
#include <opencv2/core/types.hpp>
#include <vector>

namespace cbiou {

struct DetectionBox {
    cv::Rect_<float> box;
    float score{};
    int class_id{};
};

struct TrackBox {
    int id{};
    cv::Rect_<float> box;
    float score{};
    int class_id{};
};

// Expands a box on every side by scale * width / scale * height.
cv::Rect_<float> bufferBox(const cv::Rect_<float> &box, float scale);

// IoU of two boxes after both have been buffered by the same scale.
float bufferedIoU(const cv::Rect_<float> &a, const cv::Rect_<float> &b, float scale);

class CBIoUTrack {
  public:
    CBIoUTrack(const cv::Rect_<float> &box, float score, int class_id, int motion_n);

    // Mean-displacement motion model: last observation plus the average
    // per-frame displacement over the buffered observation history.
    cv::Rect_<float> predict() const;

    void update(const cv::Rect_<float> &box, float score, int class_id);
    void markMissed();

    const cv::Rect_<float> &lastObservation() const { return observations_.back(); }

    int id{};
    int time_since_update{};
    int hits{};
    int hit_streak{};
    float score{};
    int class_id{};

    static void resetIdCounter();

  private:
    static int id_counter_;

    std::deque<cv::Rect_<float>> observations_;
    size_t motion_n_{5};
};

class CBIoUTracker {
  public:
    CBIoUTracker(int max_age = 30, int min_hits = 3, float iou_threshold = 0.3f, float b1 = 0.3f, float b2 = 0.5f,
                 int motion_n = 5);

    std::vector<TrackBox> update(const std::vector<DetectionBox> &detections);

  private:
    // One cascade round: Hungarian assignment over 1 - bufferedIoU at `buffer`,
    // restricted to the tracks and detections still unmatched.
    void associate(const std::vector<DetectionBox> &detections, const std::vector<cv::Rect_<float>> &predictions,
                   float buffer, std::vector<int> &detection_for_track, std::vector<bool> &detection_matched) const;

    int max_age_;
    int min_hits_;
    float iou_threshold_;
    float b1_;
    float b2_;
    int motion_n_;
    int frame_count_{};

    std::vector<CBIoUTrack> tracks_;
};

} // namespace cbiou
