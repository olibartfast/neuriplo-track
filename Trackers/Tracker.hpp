#pragma once
#include <vector>
#include <opencv2/core/types.hpp>



// definition of a tracking bbox
struct TrackingBox
{
    int id;
    cv::Rect_<float> box;
};


class Tracker {
public:
  virtual std::vector<TrackingBox> track(std::vector<TrackingBox>& detFrameData) = 0;
  virtual ~Tracker() = default;

};