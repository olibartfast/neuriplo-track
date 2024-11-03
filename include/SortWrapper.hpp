#pragma once
#include "BaseTracker.hpp"
#include "Sort.hpp" 
#include "Detection.hpp"
#include "TrackedObject.hpp"


class SortWrapper : public BaseTracker {
private:
    Sort tracker;
    std::set<int> classes_to_track;
public:
    SortWrapper(const std::set<int>& classes_to_track) : classes_to_track{classes_to_track} {}

    std::vector<TrackedObject> update(const std::vector<Detection>& detections) override {
        // Convert Detection to ByteTrack's format if needed and use the update method

        std::vector<TrackingBox> detection_frame_data = convertBbox(detections);
        const auto sortTrackResults = tracker.update(detection_frame_data);
        std::vector<TrackedObject> results;
        for (const auto& res : sortTrackResults) 
        {
            TrackedObject obj;
            obj.track_id = res.id;
            obj.x = res.box.x;
            obj.y = res.box.y;
            obj.width = res.box.width;
            obj.height = res.box.height;
            results.push_back(obj);
        }
        return results;
    }

    std::vector<TrackingBox> convertBbox(const std::vector<Detection>& detection_results)
    {
        // form detection box data for tracking
        std::vector<TrackingBox> detection_frame_data;
        for(auto detection_result : detection_results) {

            if (classes_to_track.find(detection_result.class_index) == classes_to_track.end()) {
                continue;
            }

            // change box to (xmin,ymin,xmax,ymax) format
            float x_min = detection_result.x;
            float y_min = detection_result.y;
            float x_max = detection_result.x + detection_result.width;
            float y_max = detection_result.y + detection_result.height;

            TrackingBox tb;
            tb.id = detection_result.class_index;
            tb.box = cv::Rect_<float>(cv::Point_<float>(x_min, y_min), cv::Point_<float>(x_max, y_max));
            detection_frame_data.push_back(tb);
        }

        return detection_frame_data;
    }

};