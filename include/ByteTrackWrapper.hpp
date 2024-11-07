#pragma once
#include "BaseTracker.hpp"
#include "ByteTrack/BYTETracker.h" 
#include "Detection.hpp"
#include "TrackedObject.hpp"



class ByteTrackWrapper : public BaseTracker {
private:
    byte_track::BYTETracker tracker;
    std::set<int> classes_to_track;
public:
    ByteTrackWrapper(const std::set<int>& classes_to_track) : classes_to_track{classes_to_track} {}


    std::vector<byte_track::Object> convertBbox(const std::vector<Detection>& detection_results)
    {
        // form detection box data for tracking
        std::vector<byte_track::Object> byte_track_objects;
        for(auto detection_result : detection_results) {

            if (classes_to_track.find(detection_result.class_index) == classes_to_track.end()) {
                continue;
            }

            // change box to (xmin,ymin,xmax,ymax) format
            float x_min = detection_result.x;
            float y_min = detection_result.y;
            float x_max = detection_result.x + detection_result.width;
            float y_max = detection_result.y + detection_result.height;

            // Rect<float> rect;
            // int label;
            // float prob;
            byte_track::Object obj(
                byte_track::Rect<float>(x_min, y_min, x_max - x_min, y_max - y_min),  
                detection_result.class_index,                                       
                detection_result.confidence                                         
            );
            byte_track_objects.emplace_back(obj);
        }

        return byte_track_objects;
    }    

    std::vector<TrackedObject> update(const std::vector<Detection>& detections, const cv::Mat &frame = cv::Mat()) override {
        // Convert Detection to ByteTrack's format if needed and use the update method
        const auto detections_byteTrack = convertBbox(detections);
       const auto byteTrackResults = tracker.update(detections_byteTrack);

        std::vector<TrackedObject> results;
        for (const auto& res : byteTrackResults) 
        {
            TrackedObject obj;
            const auto &rect = res->getRect();
            const auto &track_id = res->getTrackId();

            obj.track_id = track_id;
            obj.x = rect.x();
            obj.y = rect.y();
            obj.width = rect.width();
            obj.height = rect.height();

            results.push_back(obj);
        }
        return results;
    }
};