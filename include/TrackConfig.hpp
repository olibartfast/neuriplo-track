#pragma once
#include <set>
#include <string>

struct TrackConfig {
    std::set<int> classes_to_track;
    std::string tracker_config_path;
    std::string gmc_config_path;
    std::string reid_config_path;
    std::string reid_onnx_model_path;

    TrackConfig(const std::set<int>& classes = {}, 
                const std::string& trackerPath = "", 
                const std::string& gmcPath = "", 
                const std::string& reidPath = "", 
                const std::string& onnxPath = "")
        : classes_to_track(classes), 
          tracker_config_path(trackerPath), 
          gmc_config_path(gmcPath), 
          reid_config_path(reidPath), 
          reid_onnx_model_path(onnxPath) {}
};
