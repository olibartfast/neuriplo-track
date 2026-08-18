//
// MaskOverlap.cpp: binary-mask overlap implementation.
//
#include "MaskOverlap.hpp"

#include <algorithm>
#include <neuriplo/tasks/core/opencv_interop.hpp>
#include <opencv2/imgproc.hpp>

namespace tracking {
namespace {

// Reduces whatever the postprocessor produced to a single-channel 0/255 mask.
bool toBinaryMask(const cv::Mat &source, cv::Mat &binary) {
    if (source.empty() || source.channels() != 1) {
        return false;
    }

    cv::Mat single = source;
    if (single.type() != CV_8U) {
        single.convertTo(single, CV_8U);
    }

    // Producers emit 0/255, but binarising makes the bitwise intersection below
    // correct for any non-zero encoding.
    cv::threshold(single, binary, 0.0, 255.0, cv::THRESH_BINARY);
    return true;
}

} // namespace

MaskRegion maskRegionFrom(const neuriplo_tasks::InstanceSegmentation &segmentation) {
    MaskRegion region;

    if (segmentation.mask.empty()) {
        return region;
    }

    cv::Mat binary;
    if (!toBinaryMask(neuriplo_tasks::toCvMat(segmentation.mask), binary)) {
        return region;
    }

    const cv::Rect box(segmentation.bbox.x, segmentation.bbox.y, segmentation.bbox.width, segmentation.bbox.height);

    if (box.width > 0 && box.height > 0 && box.x >= 0 && box.y >= 0 && box.x + box.width <= binary.cols &&
        box.y + box.height <= binary.rows) {
        // The expected case: a full-frame mask whose foreground lies inside the
        // detection box.
        region.mask = binary(box).clone();
        region.origin = box.tl();
    } else if (box.width == binary.cols && box.height == binary.rows) {
        // Already cropped to the box by the producer.
        region.mask = binary.clone();
        region.origin = cv::Point(segmentation.bbox.x, segmentation.bbox.y);
    } else {
        // Unrecognised geometry: use the mask as given, anchored at the origin.
        region.mask = binary.clone();
        region.origin = cv::Point(0, 0);
    }

    region.foreground = cv::countNonZero(region.mask);
    return region;
}

float maskIoU(const MaskRegion &a, const MaskRegion &b) {
    if (a.empty() || b.empty()) {
        return 0.0f;
    }

    const cv::Rect overlap = a.bounds() & b.bounds();
    if (overlap.width <= 0 || overlap.height <= 0) {
        return 0.0f;
    }

    cv::Mat intersection;
    cv::bitwise_and(a.mask(overlap - a.origin), b.mask(overlap - b.origin), intersection);

    const int intersection_area = cv::countNonZero(intersection);
    if (intersection_area == 0) {
        return 0.0f;
    }

    const int union_area = a.foreground + b.foreground - intersection_area;
    return union_area > 0 ? static_cast<float>(intersection_area) / static_cast<float>(union_area) : 0.0f;
}

float blendOverlap(float box_iou, const MaskRegion &a, const MaskRegion &b, float mask_weight) {
    if (mask_weight <= 0.0f || a.empty() || b.empty()) {
        return box_iou;
    }

    const float weight = std::min(mask_weight, 1.0f);
    return (1.0f - weight) * box_iou + weight * maskIoU(a, b);
}

} // namespace tracking
