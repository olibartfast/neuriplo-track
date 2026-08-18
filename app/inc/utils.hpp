#pragma once
#include "AppConfig.hpp"
#include "TrackConfig.hpp"

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>

std::vector<std::string> readLabelNames(const std::string &fileName);
std::vector<std::string> splitString(const std::string &s, char delimiter);
std::string generateOutputPath(const std::string &inputPath);
cv::VideoWriter setupVideoWriter(const cv::VideoCapture &cap, const std::string &outputPath);

// Maps a user-supplied tracker name to its canonical spelling ("OC-SORT" and
// "ocsort" both become "OCSORT"). Returns an empty string for unknown names.
std::string canonicalTrackerName(const std::string &trackingAlgorithm);

// Builds the tracker configuration: TrackConfig defaults, then the defaults of
// the selected algorithm, then whatever was set on the command line.
TrackConfig makeTrackConfig(const AppConfig &config);

// Checks tracker tuning values that were given on the command line. Returns the
// first problem found, or an empty optional when every value is usable. Kept
// separate from the parser so it can be tested without exiting the process.
std::optional<std::string> validateTrackerOverrides(const TrackerOverrides &overrides);
