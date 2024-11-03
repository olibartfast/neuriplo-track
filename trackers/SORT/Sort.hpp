//
// Sort.h: SORT(Simple Online and Realtime Tracking) Class Declaration
//
#pragma once
#include <vector>
#include <opencv2/core/types.hpp>
#include "Detector.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "Hungarian.hpp"
#include "KalmanTracker.hpp"
#include <set>
#include <iomanip> // to format image names using setw() and setfill()

// definition of a tracking bbox
struct TrackingBox
{
    int id;
    cv::Rect_<float> box;
};


// This class represents the internel state of individual tracked objects observed as bounding box.
class Sort{
public:
    Sort()
    {
        m_max_age = 5;
        m_min_hits = 3;
        m_iou_threshold = 0.3;
        m_frame_count = 0;
    }

    Sort(int max_age, int min_hits, double iou_threshold)
        : m_max_age(max_age),
          m_min_hits(min_hits),
          m_iou_threshold(iou_threshold)
    {
        m_frame_count = 0;
    }

    ~Sort() {}

    std::vector<TrackingBox> update(const std::vector<TrackingBox>& detect_frame_data);
private:
    int m_max_age; //  maximum number of consecutive frames that an object can go undetected before it is considered to have left the scene. 
    int m_min_hits; // minimum number of times that an object must be detected before it is considered to be a valid track
    double m_iou_threshold;
    int m_frame_count;
    std::vector<KalmanTracker> m_trackers;
    std::vector<TrackingBox> m_tracking_output;
};
