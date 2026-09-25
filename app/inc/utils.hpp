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

// Checks the combinations of evaluation and output flags. Returns the first
// problem found, or an empty optional when the configuration is coherent.
// Kept pure, and separate from the parser, so the rules are testable without
// spawning a process -- the same reasoning as validateTrackerOverrides.
std::optional<std::string> validateEvaluationOptions(const AppConfig &config);

// True when the run should render and write a video. A video is written when
// --output was given, and otherwise only when the run was not asked for any
// other artefact -- so an invocation that predates the evaluation flags keeps
// producing its auto-named video, while `--mot_output out.txt` alone does not
// pay for H.264 encoding.
bool shouldWriteVideo(const AppConfig &config);
