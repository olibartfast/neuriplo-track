#pragma once
#include <vector>
#include <opencv2/core/types.hpp>
#include "Detection.hpp"
#include "TrackedObject.hpp"



class BaseTracker {
public:
    virtual ~BaseTracker() = default;
    virtual std::vector<TrackedObject> update(const std::vector<Detection>& detections) = 0;
};

