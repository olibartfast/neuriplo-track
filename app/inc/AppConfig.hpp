#pragma once
#include <string>
#include <vector>
#include <set>

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
    
    // Detection/inference settings
    bool use_gpu;
    float confidenceThreshold;
    int batch_size;
    std::vector<std::vector<int64_t>> input_sizes;
    
    // Output settings
    std::string outputPath;
    bool displayOutput;
};
