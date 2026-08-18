//
// MaskOverlap.hpp: binary-mask overlap used as an association cue.
//
// neuriplo-tasks postprocessors hand back a full-frame single-channel 0/255
// mask whose foreground never leaves the detection's bounding box. Keeping one
// full-frame mask per live track would cost megabytes at 1080p, so a mask is
// stored cropped to its box together with the box origin, and overlap is
// computed only where two such regions intersect.
//
#pragma once
#include <neuriplo/tasks/core/result_types.hpp>
#include <opencv2/core/mat.hpp>

namespace tracking {

// A binary mask crop and where it sits in the frame.
struct MaskRegion {
    cv::Mat mask;           // CV_8U, binarised to 0/255
    cv::Point origin{0, 0}; // top-left corner of `mask` in frame coordinates
    int foreground{0};      // non-zero pixel count, cached for the union term

    bool empty() const { return mask.empty() || foreground == 0; }
    cv::Rect bounds() const { return cv::Rect(origin, mask.size()); }
};

// Crops a segmentation result's full-frame mask to its bounding box. Returns an
// empty region when the result carries no usable mask, which callers treat as
// "no mask cue available" rather than "no overlap".
MaskRegion maskRegionFrom(const neuriplo_tasks::InstanceSegmentation &segmentation);

// Intersection over union of two binary masks, in [0, 1]. Empty regions and
// regions whose bounds do not intersect give 0 without touching pixels.
float maskIoU(const MaskRegion &a, const MaskRegion &b);

// Blends a box overlap with the corresponding mask overlap:
//   (1 - mask_weight) * box_iou + mask_weight * maskIoU(a, b)
// A pair where either side carries no mask keeps its box score untouched, so a
// stream in which only some detections are masked still associates normally.
// mask_weight <= 0 returns box_iou unchanged.
float blendOverlap(float box_iou, const MaskRegion &a, const MaskRegion &b, float mask_weight);

} // namespace tracking
