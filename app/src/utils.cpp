#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <glog/logging.h>
#include <iostream>
#include <sstream>

std::vector<std::string> readLabelNames(const std::string &fileName) {
    if (!std::filesystem::exists(fileName)) {
        std::cerr << "Wrong path to labels: " << fileName << std::endl;
        exit(1);
    }

    std::vector<std::string> classes;
    std::ifstream ifs(fileName);
    std::string line;
    while (getline(ifs, line)) {
        classes.push_back(line);
    }
    return classes;
}

std::vector<std::string> splitString(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::string generateOutputPath(const std::string &inputPath) {
    std::filesystem::path inputFilePath(inputPath);
    if (inputFilePath.extension().empty()) {
        return "output_processed.mp4";
    }
    return inputFilePath.stem().string() + "_processed" + inputFilePath.extension().string();
}

std::string canonicalTrackerName(const std::string &trackingAlgorithm) {
    // Compare ignoring case and the separators used by the papers' spellings,
    // so "OC-SORT", "OCSORT" and "ocsort" all name the same tracker.
    std::string key;
    key.reserve(trackingAlgorithm.size());
    for (const char c : trackingAlgorithm) {
        if (c == '-' || c == '_' || c == ' ') {
            continue;
        }
        key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (key == "sort") {
        return "SORT";
    }
    if (key == "bytetrack") {
        return "ByteTrack";
    }
    if (key == "botsort") {
        return "BoTSORT";
    }
    if (key == "ocsort") {
        return "OCSORT";
    }
    if (key == "cbiou") {
        return "CBIoU";
    }
    return "";
}

std::optional<std::string> validateTrackerOverrides(const TrackerOverrides &overrides) {
    const auto belowMinimum = [](const char *flag, const std::optional<int> &value,
                                 int minimum) -> std::optional<std::string> {
        if (value && *value < minimum) {
            return std::string("--") + flag + " must be at least " + std::to_string(minimum) + " (got " +
                   std::to_string(*value) + ")";
        }
        return std::nullopt;
    };
    const auto outsideRange = [](const char *flag, const std::optional<float> &value, float low,
                                 float high) -> std::optional<std::string> {
        if (value && (!(*value >= low) || !(*value <= high))) {
            return std::string("--") + flag + " must be between " + std::to_string(low) + " and " +
                   std::to_string(high) + " (got " + std::to_string(*value) + ")";
        }
        return std::nullopt;
    };

    const std::optional<std::string> problems[] = {
        belowMinimum("max_age", overrides.max_age, 0),
        belowMinimum("min_hits", overrides.min_hits, 1),
        outsideRange("iou_threshold", overrides.iou_threshold, 0.0f, 1.0f),
        belowMinimum("track_buffer", overrides.track_buffer, 1),
        outsideRange("track_thresh", overrides.track_thresh, 0.0f, 1.0f),
        outsideRange("high_thresh", overrides.high_thresh, 0.0f, 1.0f),
        outsideRange("match_thresh", overrides.match_thresh, 0.0f, 1.0f),
        belowMinimum("delta_t", overrides.ocsort_delta_t, 1),
        outsideRange("inertia", overrides.ocsort_inertia, 0.0f, 1.0f),
        outsideRange("det_thresh", overrides.ocsort_det_thresh, 0.0f, 1.0f),
        outsideRange("biou_b1", overrides.cbiou_b1, 0.0f, 5.0f),
        outsideRange("biou_b2", overrides.cbiou_b2, 0.0f, 5.0f),
        belowMinimum("motion_n", overrides.cbiou_motion_n, 1),
    };

    for (const auto &problem : problems) {
        if (problem) {
            return problem;
        }
    }

    // The cascade searches a tight radius first and a wider one second; the
    // reverse order is not an error, but it is almost certainly a mistake.
    if (overrides.cbiou_b1 && overrides.cbiou_b2 && *overrides.cbiou_b1 > *overrides.cbiou_b2) {
        LOG(WARNING) << "--biou_b1 (" << *overrides.cbiou_b1 << ") is larger than --biou_b2 (" << *overrides.cbiou_b2
                     << "); the second cascade round is meant to search the wider radius";
    }

    return std::nullopt;
}

TrackConfig makeTrackConfig(const AppConfig &config) {
    TrackConfig trackConfig(config.classesToTrackIds, config.trackerConfigPath, config.gmcConfigPath,
                            config.reidConfigPath, config.reidOnnxPath);

    // OC-SORT and C-BIoU are built around recovering a track across a gap, which
    // the shared TrackConfig default of max_age = 1 would make impossible. Their
    // paper defaults apply unless the command line says otherwise.
    const std::string algorithm = canonicalTrackerName(config.trackingAlgorithm);
    if (algorithm == "OCSORT" || algorithm == "CBIoU") {
        trackConfig.max_age = 30;
        trackConfig.min_hits = 3;
        trackConfig.iou_threshold = 0.3f;
    }

    const TrackerOverrides &overrides = config.trackerOverrides;
    if (overrides.max_age) {
        trackConfig.max_age = *overrides.max_age;
    }
    if (overrides.min_hits) {
        trackConfig.min_hits = *overrides.min_hits;
    }
    if (overrides.iou_threshold) {
        trackConfig.iou_threshold = *overrides.iou_threshold;
    }
    if (overrides.track_buffer) {
        trackConfig.track_buffer = *overrides.track_buffer;
    }
    if (overrides.track_thresh) {
        trackConfig.track_thresh = *overrides.track_thresh;
    }
    if (overrides.high_thresh) {
        trackConfig.high_thresh = *overrides.high_thresh;
    }
    if (overrides.match_thresh) {
        trackConfig.match_thresh = *overrides.match_thresh;
    }
    if (overrides.ocsort_delta_t) {
        trackConfig.ocsort_delta_t = *overrides.ocsort_delta_t;
    }
    if (overrides.ocsort_inertia) {
        trackConfig.ocsort_inertia = *overrides.ocsort_inertia;
    }
    if (overrides.ocsort_det_thresh) {
        trackConfig.ocsort_det_thresh = *overrides.ocsort_det_thresh;
    }
    if (overrides.cbiou_b1) {
        trackConfig.cbiou_b1 = *overrides.cbiou_b1;
    }
    if (overrides.cbiou_b2) {
        trackConfig.cbiou_b2 = *overrides.cbiou_b2;
    }
    if (overrides.cbiou_motion_n) {
        trackConfig.cbiou_motion_n = *overrides.cbiou_motion_n;
    }

    return trackConfig;
}

cv::VideoWriter setupVideoWriter(const cv::VideoCapture &cap, const std::string &outputPath) {
    cv::Size frame_size(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
                        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    double fps = cap.get(cv::CAP_PROP_FPS);
    return cv::VideoWriter(outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frame_size);
}
