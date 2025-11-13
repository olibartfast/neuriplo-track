#pragma once
#include "BaseTracker.hpp"
#include "BoTSORT.h"
#include <memory>

class BoTSORTWrapper : public BaseTracker {
private:
    std::unique_ptr<botsort::BoTSORT> tracker_;

public:
    explicit BoTSORTWrapper(const TrackConfig& config);
    ~BoTSORTWrapper() override;
    
    std::vector<TrackedObject> update(const std::vector<Detection>& detections, const cv::Mat& frame = cv::Mat()) override;
};
