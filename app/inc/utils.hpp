#pragma once
#include "AppConfig.hpp"
#include "TrackConfig.hpp"

#include <opencv2/opencv.hpp>
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
