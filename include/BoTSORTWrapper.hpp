#include "BaseTracker.hpp"
#include "BoTSORT.h"


class BotSORTWrapper : public BaseTracker {
private:
    botsort::BoTSORT tracker;
    std::set<int> classes_to_track;  // Added missing member variable

public:
    BotSORTWrapper(const std::set<int>& classes_to_track) : 
    classes_to_track(classes_to_track), 
    tracker("", "","","") {}  // Fixed initializer syntax

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
            if (classes_to_track.find(detection_result.class_index) == classes_to_track.end()) {
                continue;
            }

            // Change box to (xmin,ymin,xmax,ymax) format
            float x_min = detection_result.x;
            float y_min = detection_result.y;
            float x_max = detection_result.x + detection_result.width;
            float y_max = detection_result.y + detection_result.height;

            botsort::Detection botsort_detection;
            botsort_detection.bbox_tlwh = cv::Rect_<float>(cv::Point_<float>(x_min, y_min), cv::Point_<float>(x_max, y_max));
            botsort_detection.class_id = detection_result.class_index;
            botsort_detection.confidence = detection_result.confidence;

            botsort_detections.push_back(botsort_detection);    
        }

        return botsort_detections; 
    }
};  // Added missing semicolon after class definition