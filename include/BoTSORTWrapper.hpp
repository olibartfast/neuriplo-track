#include "BaseTracker.hpp"
#include "BoTSORT.h"


// struct BoTSORTConfig : public TrackConfig {
//     std::string tracker_config_path;
//     std::string gmc_config_path;
//     std::string reid_config_path;
//     std::string reid_onnx_model_path;

//     BoTSORTConfig(const std::set<int>& classes = {}, 
//                   const std::string& trackerPath = "config/tracker.ini", 
//                   const std::string& gmcPath = "config/gmc.ini", 
//                   const std::string& reidPath = "config/reid.ini", 
//                   const std::string& onnxPath = "")
//         : TrackConfig(classes), 
//           tracker_config_path(trackerPath), 
//           gmc_config_path(gmcPath), 
//           reid_config_path(reidPath), 
//           reid_onnx_model_path(onnxPath) {}
// };

class BoTSORTWrapper : public BaseTracker {
private:
    botsort::BoTSORT tracker;
    std::set<int> classes_to_track; 

public:
    BoTSORTWrapper(const TrackConfig& config)
        : classes_to_track(config.classes_to_track),
          tracker(config.tracker_config_path, config.gmc_config_path, config.reid_config_path, config.reid_onnx_model_path) 
    {}

    std::vector<TrackedObject> update(const std::vector<Detection>& detections, const cv::Mat& frame = cv::Mat()) override {
        // Convert tracking results back to TrackedObject format
        // TODO: Implement actual tracking logic using tracker.update()

        // Convert Detection to BoTSORT's format if needed and use the update method
        std::vector<botsort::Detection> botsort_detections = convertBbox(detections);
        const auto botsort_track_results = tracker.track(botsort_detections, frame);

        std::vector<TrackedObject> results;
        for (const auto& res : botsort_track_results)
        {
            TrackedObject obj;
            obj.track_id = res->track_id;
            const auto tlwh = res->get_tlwh();
            obj.x = tlwh[0];
            obj.y = tlwh[1];
            obj.width = tlwh[2];
            obj.height = tlwh[3];
            results.push_back(obj);
        }
        return results;
    }

    std::vector<botsort::Detection> convertBbox(const std::vector<Detection>& detection_results) {
        // Form detection box data for tracking
        std::vector<botsort::Detection> botsort_detections;
        
        for(const auto& detection_result : detection_results) {  // Added const reference
            if (classes_to_track.find(detection_result.label) == classes_to_track.end()) {
                continue;
            }



            botsort::Detection botsort_detection;
            botsort_detection.bbox_tlwh = detection_result.bbox;
            botsort_detection.class_id = detection_result.label;
            botsort_detection.confidence = detection_result.score;

            botsort_detections.push_back(botsort_detection);    
        }

        return botsort_detections; 
    }
};  // Added missing semicolon after class definition
