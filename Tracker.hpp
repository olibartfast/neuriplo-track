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
  virtual void track(std::vector<TrackingBox>& detFrameData) = 0;
  virtual std::vector<TrackingBox> getTrackingBoxes() = 0;
  virtual ~Tracker() = default;

 // Rule of Five implementation
//   Tracker() = default; 
//   Tracker(const Tracker& other) = default; 
//   Tracker& operator=(const Tracker& other) = default; 
//   Tracker(Tracker&& other) = default; 
//   Tracker& operator=(Tracker&& other) = default; 

};