#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

std::vector<std::string> readLabelNames(const std::string& fileName);
std::vector<std::string> splitString(const std::string& s, char delimiter);
std::string generateOutputPath(const std::string& inputPath);
cv::VideoWriter setupVideoWriter(const cv::VideoCapture& cap, const std::string& outputPath);
