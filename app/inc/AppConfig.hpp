#pragma once
#include <optional>
#include <set>
#include <string>
#include <vector>

// Tracker parameters explicitly provided on the command line. Unset entries fall
// back to the per-algorithm defaults resolved in makeTrackConfig().
struct TrackerOverrides {
    std::optional<int> max_age;
    std::optional<int> min_hits;
    std::optional<float> iou_threshold;

    std::optional<int> track_buffer;
    std::optional<float> track_thresh;
    std::optional<float> high_thresh;
    std::optional<float> match_thresh;

    std::optional<int> ocsort_delta_t;
    std::optional<float> ocsort_inertia;
    std::optional<float> ocsort_det_thresh;

    std::optional<float> cbiou_b1;
    std::optional<float> cbiou_b2;
    std::optional<int> cbiou_motion_n;

    std::optional<float> mask_iou_weight;
};

struct AppConfig {
    std::string detectorType;
    std::string source;
    std::string labelsPath;
    std::string weights;
    std::string trackingAlgorithm;
    std::vector<std::string> classesToTrack;
    std::set<int> classesToTrackIds;

    // Tracker configuration paths
    std::string trackerConfigPath;
    std::string gmcConfigPath;
    std::string reidConfigPath;
    std::string reidOnnxPath;

    // Tracker tuning parameters set on the command line
    TrackerOverrides trackerOverrides;

    // Detection/inference settings
    bool use_gpu;
    float confidenceThreshold;
    int batch_size;
    std::vector<std::vector<int64_t>> input_sizes;

    // Output settings
    std::string outputPath;
    bool displayOutput;

    // Evaluation. Every one of these is empty when the flag was not passed.
    //
    // motOutputPath       where tracking results go, MOTChallenge format
    // saveDetectionsPath  where the detections fed to the tracker are captured
    // detectionsPath      a captured file to replay instead of running a detector
    //
    // A run is a *replay* when detectionsPath is set: no detector, no inference
    // engine and no backend are constructed at all.
    std::string motOutputPath;
    std::string saveDetectionsPath;
    std::string detectionsPath;

    [[nodiscard]] bool isReplay() const { return !detectionsPath.empty(); }
};
