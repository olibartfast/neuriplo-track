#pragma once
#include <vector>
#include <opencv2/core/types.hpp>
#include "Detector.hpp"



// definition of a tracking bbox
struct TrackingBox
{
    int id;
    cv::Rect_<float> box;
};


class Tracker {
public:
  virtual std::vector<TrackingBox> track(const std::vector<t_prediction>& detection_results, 
    const std::vector<std::string>& classes, 
    const std::vector<std::string>& track_classes) = 0;
  virtual ~Tracker() = default;

std::vector<TrackingBox> convertBbox(const std::vector<t_prediction>& detection_results, 
    const std::vector<std::string>& classes, 
    const std::vector<std::string>& track_classes)
{
    // form detection box data for tracking
    std::vector<TrackingBox> detection_frame_data;
    for(auto detection_result : detection_results) {

        // check tracking class list if needed
        std::string detection_class = classes[detection_result.class_index];
        if(std::find(track_classes.begin(), track_classes.end(), detection_class) == track_classes.end()) {
            continue;
        }

        // change box to (xmin,ymin,xmax,ymax) format
        float x_min = detection_result.x;
        float y_min = detection_result.y;
        float x_max = detection_result.x + detection_result.width;
        float y_max = detection_result.y + detection_result.height;

        TrackingBox tb;
        tb.box = cv::Rect_<float>(cv::Point_<float>(x_min, y_min), cv::Point_<float>(x_max, y_max));
        detection_frame_data.push_back(tb);
    }

    return detection_frame_data;
}

};