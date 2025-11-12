#pragma once
#include "BaseTracker.hpp"
#include "BYTETracker.h"
#include <memory>

class BYTETracker;

class ByteTrackWrapper : public BaseTracker {
private:
    std::unique_ptr<BYTETracker> tracker_;

public:
    explicit ByteTrackWrapper(const TrackConfig& config);
    ~ByteTrackWrapper() override;
    
    std::vector<TrackedObject> update(const std::vector<Detection>& detections, const cv::Mat& frame = cv::Mat()) override;
};
