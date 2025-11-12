#include "utils.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

std::vector<std::string> readLabelNames(const std::string& fileName) {
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

std::vector<std::string> splitString(const std::string& s, char delimiter) {
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

std::string generateOutputPath(const std::string& inputPath) {
    std::filesystem::path inputFilePath(inputPath);
    if (inputFilePath.extension().empty()) {
        return "output_processed.mp4";
    }
    return inputFilePath.stem().string() + "_processed" + inputFilePath.extension().string();
}

cv::VideoWriter setupVideoWriter(const cv::VideoCapture& cap, const std::string& outputPath) {
    cv::Size frame_size(
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT))
    );
    double fps = cap.get(cv::CAP_PROP_FPS);
    return cv::VideoWriter(outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frame_size);
}
