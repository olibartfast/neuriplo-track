//
// MaskPropagator.hpp: seam between the SAM3 tracker loop and mask inference.
//
// A MaskPropagator advances every live object onto the current frame and
// reports, per object id, a mask plus a presence (occlusion) score. Nothing
// SAM-specific appears in it: the operations mirror the contract the upstream
// stateful video segmentation task must satisfy
// (specs/2026-10-10-sam3-video-tracker/plan.md, Group 0), so an adapter over
// that task can implement this interface without touching the tracker loop,
// and the loop stays testable with a scripted double.
//
// Identity lives in the propagator: the id handed back by addObject names the
// same object in every later propagate() call until removeObject. The tracker
// loop in trackers/SAM3 only decides which detections prompt, re-prompt or
// retire those ids.
//
#pragma once
#include "MaskOverlap.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>

namespace tracking {

// One live object on the frame propagate() just advanced.
struct PropagatedMask {
    int id{};         // propagator-assigned, stable until removeObject
    MaskRegion mask;  // crop of the full-frame mask, in frame coordinates
    float presence{}; // occlusion score in [0, 1]; below the tracker's
                      // presence threshold the object is absent
};

class MaskPropagator {
  public:
    virtual ~MaskPropagator() = default;

    // Advances every live object onto `frame` and reports one entry per live
    // object. The frame is borrowed, never retained.
    virtual std::vector<PropagatedMask> propagate(const cv::Mat &frame) = 0;

    // Starts tracking a new object from a detector box prompt on `frame`.
    // Returns the id the propagator assigned to it, which names the object
    // until removeObject. Must fail visibly past capacity(), never silently
    // evict a live object; the tracker loop never calls it past capacity.
    virtual int addObject(const cv::Rect_<float> &box, const cv::Mat &frame) = 0;

    // Feeds a detector box back as a correction prompt for a live id, keeping
    // the id and its memory. Unknown ids are ignored.
    virtual void reprompt(int id, const cv::Rect_<float> &box, const cv::Mat &frame) = 0;

    // Forgets a live object. Unknown ids are ignored.
    virtual void removeObject(int id) = 0;

    // Maximum number of simultaneously live objects. Sizes the SAM 3.1 Object
    // Multiplex buckets on the inference side.
    virtual int capacity() const = 0;
};

} // namespace tracking
