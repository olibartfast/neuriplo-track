#pragma once
#include <set>
#include <string>

struct TrackConfig {
    std::set<int> classes_to_track;
    
    // SORT parameters
    int max_age = 1;
    int min_hits = 3;
    float iou_threshold = 0.3f;
    
    // ByteTrack parameters
    int track_buffer = 30;
    float track_thresh = 0.5f;
    float high_thresh = 0.6f;
    float match_thresh = 0.8f;
    
    // BoTSORT parameters
    std::string tracker_config_path;
    std::string gmc_config_path;
    std::string reid_config_path;
    std::string reid_onnx_path;

    TrackConfig(const std::set<int>& classes = {}, 
                const std::string& trackerPath = "", 
                const std::string& gmcPath = "", 
                const std::string& reidPath = "", 
                const std::string& onnxPath = "")
        : classes_to_track(classes), 
          tracker_config_path(trackerPath), 
          gmc_config_path(gmcPath), 
          reid_config_path(reidPath), 
          reid_onnx_path(onnxPath) {}
};
