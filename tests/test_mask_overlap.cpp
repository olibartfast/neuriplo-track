//
// Binary-mask overlap: hand-computed cases.
//
#include "MaskOverlap.hpp"
#include "test_util.hpp"

#include <neuriplo/tasks/core/opencv_interop.hpp>
#include <opencv2/core.hpp>

namespace {

constexpr int kFrameWidth = 200;
constexpr int kFrameHeight = 120;

// Builds a segmentation result the way a neuriplo-tasks postprocessor does: a
// full-frame 0/255 mask whose foreground is a rectangle inside the bounding box.
neuriplo_tasks::InstanceSegmentation makeSegmentation(const cv::Rect &box, const cv::Rect &foreground) {
    neuriplo_tasks::InstanceSegmentation segmentation;
    segmentation.bbox = neuriplo_tasks::BoundingBox(box.x, box.y, box.width, box.height);
    segmentation.class_confidence = 0.9f;
    segmentation.class_id = 0.0f;

    cv::Mat full = cv::Mat::zeros(kFrameHeight, kFrameWidth, CV_8U);
    if (foreground.width > 0 && foreground.height > 0) {
        full(foreground).setTo(255);
    }
    segmentation.mask = neuriplo_tasks::fromCvMat(full);
    segmentation.mask_height = kFrameHeight;
    segmentation.mask_width = kFrameWidth;
    return segmentation;
}

void testIdenticalMasks() {
    const cv::Rect box(10, 10, 40, 40);
    const auto a = tracking::maskRegionFrom(makeSegmentation(box, box));
    const auto b = tracking::maskRegionFrom(makeSegmentation(box, box));

    CHECK(!a.empty());
    CHECK_NEAR(tracking::maskIoU(a, b), 1.0f, 1e-5f);
}

void testDisjointMasks() {
    // Boxes overlap heavily, foreground pixels do not: the case the cue exists for.
    const auto a = tracking::maskRegionFrom(makeSegmentation(cv::Rect(10, 10, 40, 40), cv::Rect(10, 10, 20, 40)));
    const auto b = tracking::maskRegionFrom(makeSegmentation(cv::Rect(12, 10, 40, 40), cv::Rect(32, 10, 20, 40)));

    CHECK_NEAR(tracking::maskIoU(a, b), 0.0f, 1e-6f);
}

void testHalfOverlap() {
    // 40x40 each, sharing a 20x40 strip: intersection 800, union 2400 -> 1/3.
    const auto a = tracking::maskRegionFrom(makeSegmentation(cv::Rect(10, 10, 40, 40), cv::Rect(10, 10, 40, 40)));
    const auto b = tracking::maskRegionFrom(makeSegmentation(cv::Rect(30, 10, 40, 40), cv::Rect(30, 10, 40, 40)));

    CHECK_NEAR(tracking::maskIoU(a, b), 1.0f / 3.0f, 1e-4f);
}

void testEmptyMask() {
    const auto empty = tracking::maskRegionFrom(makeSegmentation(cv::Rect(10, 10, 40, 40), cv::Rect()));
    const auto solid = tracking::maskRegionFrom(makeSegmentation(cv::Rect(10, 10, 40, 40), cv::Rect(10, 10, 40, 40)));

    CHECK(empty.empty());
    CHECK_NEAR(tracking::maskIoU(empty, solid), 0.0f, 1e-6f);
    CHECK_NEAR(tracking::maskIoU(solid, empty), 0.0f, 1e-6f);

    // A detection with no mask at all must read as "no cue", not as "no overlap".
    neuriplo_tasks::InstanceSegmentation without_mask;
    without_mask.bbox = neuriplo_tasks::BoundingBox(10, 10, 40, 40);
    CHECK(tracking::maskRegionFrom(without_mask).empty());
}

void testNonIntersectingBounds() {
    const auto a = tracking::maskRegionFrom(makeSegmentation(cv::Rect(0, 0, 30, 30), cv::Rect(0, 0, 30, 30)));
    const auto b = tracking::maskRegionFrom(makeSegmentation(cv::Rect(150, 80, 30, 30), cv::Rect(150, 80, 30, 30)));

    CHECK_NEAR(tracking::maskIoU(a, b), 0.0f, 1e-6f);
}

// Cropping to the bounding box must not change the answer.
void testCroppingIsLossless() {
    const cv::Rect box_a(10, 10, 40, 40);
    const cv::Rect box_b(30, 10, 40, 40);
    const cv::Rect fg_a(15, 15, 30, 30);
    const cv::Rect fg_b(35, 15, 30, 30);

    const auto a = tracking::maskRegionFrom(makeSegmentation(box_a, fg_a));
    const auto b = tracking::maskRegionFrom(makeSegmentation(box_b, fg_b));

    // Same computation over the uncropped frames.
    cv::Mat full_a = cv::Mat::zeros(kFrameHeight, kFrameWidth, CV_8U);
    cv::Mat full_b = cv::Mat::zeros(kFrameHeight, kFrameWidth, CV_8U);
    full_a(fg_a).setTo(255);
    full_b(fg_b).setTo(255);

    cv::Mat intersection;
    cv::bitwise_and(full_a, full_b, intersection);
    const float expected =
        static_cast<float>(cv::countNonZero(intersection)) /
        static_cast<float>(cv::countNonZero(full_a) + cv::countNonZero(full_b) - cv::countNonZero(intersection));

    CHECK_NEAR(tracking::maskIoU(a, b), expected, 1e-5f);
}

} // namespace

int main() {
    testIdenticalMasks();
    testDisjointMasks();
    testHalfOverlap();
    testEmptyMask();
    testNonIntersectingBounds();
    testCroppingIsLossless();
    return test_util::summary("mask_overlap");
}
