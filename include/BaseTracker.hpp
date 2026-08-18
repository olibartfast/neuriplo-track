#pragma once
#include "TrackConfig.hpp"
#include "TrackedObject.hpp"

#include <neuriplo/tasks/core/result_types.hpp>
#include <opencv2/core/mat.hpp>
#include <vector>

class BaseTracker {
  protected:
    TrackConfig config_;

  public:
    explicit BaseTracker(const TrackConfig &config) : config_(config) {}
    virtual ~BaseTracker() = default;
    virtual std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::Detection> &detections,
                                              const cv::Mat &frame = cv::Mat()) = 0;

    // Segmentation-model overload. The default slices each result to its
    // Detection base and delegates, so a tracker that has no use for masks needs
    // no code at all; trackers that do override this.
    //
    // Note this overload is hidden in a derived class that declares only the
    // Detection one, which is harmless as long as trackers are used through a
    // BaseTracker reference, as the application does.
    virtual std::vector<TrackedObject> update(const std::vector<neuriplo_tasks::InstanceSegmentation> &segmentations,
                                              const cv::Mat &frame = cv::Mat()) {
        std::vector<neuriplo_tasks::Detection> detections;
        detections.reserve(segmentations.size());
        for (const auto &segmentation : segmentations) {
            detections.push_back(static_cast<const neuriplo_tasks::Detection &>(segmentation));
        }
        return update(detections, frame);
    }
};
