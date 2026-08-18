//
// Tracker behaviour against scripted detection sequences.
//
// Every case builds neuriplo_tasks::Detection frames by hand, so no model, no
// weights and no video are involved: what is under test is the association and
// track-management logic, not a detector.
//
#include "ByteTrackWrapper.hpp"
#include "CBIoUWrapper.hpp"
#include "OCSortWrapper.hpp"
#include "SortWrapper.hpp"
#include "test_util.hpp"

#include <functional>
#include <map>
#include <memory>
#include <neuriplo/tasks/core/opencv_interop.hpp>
#include <opencv2/core.hpp>
#include <set>
#include <string>
#include <vector>

namespace {

using TrackerFactory = std::function<std::unique_ptr<BaseTracker>(const TrackConfig &)>;

struct NamedTracker {
    std::string name;
    TrackerFactory factory;
    // Only the trackers configured for it are expected to recover an identity
    // across a detection gap.
    bool recovers_after_gap;
};

neuriplo_tasks::Detection makeDetection(int x, int y, int width, int height, float confidence, int class_id) {
    neuriplo_tasks::Detection detection;
    detection.bbox = neuriplo_tasks::BoundingBox(x, y, width, height);
    detection.class_confidence = confidence;
    detection.class_id = static_cast<float>(class_id);
    return detection;
}

TrackConfig baseConfig(int max_age, int min_hits = 3) {
    TrackConfig config({0});
    config.max_age = max_age;
    config.min_hits = min_hits;
    config.iou_threshold = 0.3f;
    return config;
}

std::vector<NamedTracker> allTrackers() {
    return {
        {"SORT", [](const TrackConfig &c) { return std::unique_ptr<BaseTracker>(new SortWrapper(c)); }, false},
        {"ByteTrack", [](const TrackConfig &c) { return std::unique_ptr<BaseTracker>(new ByteTrackWrapper(c)); },
         false},
        {"OCSORT", [](const TrackConfig &c) { return std::unique_ptr<BaseTracker>(new OCSortWrapper(c)); }, true},
        {"CBIoU", [](const TrackConfig &c) { return std::unique_ptr<BaseTracker>(new CBIoUWrapper(c)); }, true},
    };
}

// A single box travelling at constant velocity must end up under exactly one
// track id, and that id must not change while the object is visible.
void testSteadyLinearMotion() {
    for (const auto &entry : allTrackers()) {
        auto tracker = entry.factory(baseConfig(30));

        std::set<int> ids_after_warmup;
        for (int frame = 0; frame < 20; ++frame) {
            const std::vector<neuriplo_tasks::Detection> detections = {
                makeDetection(20 + 5 * frame, 100, 60, 60, 0.9f, 0)};
            const std::vector<TrackedObject> tracks = tracker->update(detections);

            if (frame >= 6) {
                CHECK_LABELED(tracks.size() == 1, entry.name);
                for (const auto &track : tracks) {
                    ids_after_warmup.insert(track.track_id);
                }
            }
        }

        CHECK_LABELED(ids_after_warmup.size() == 1, entry.name);
    }
}

// The object disappears for five frames and comes back where its motion says it
// should be. OC-SORT (ORU/OCR) and C-BIoU (buffered matching) are expected to
// hand it back its original id; SORT and ByteTrack are exercised for crashes
// only, since their configured lifetime makes no such promise.
void testOcclusionGap() {
    constexpr int kGapStart = 8;
    constexpr int kGapFrames = 5;

    for (const auto &entry : allTrackers()) {
        auto tracker = entry.factory(baseConfig(30));

        int id_before_gap = -1;
        std::set<int> ids_after_gap;

        for (int frame = 0; frame < 24; ++frame) {
            std::vector<neuriplo_tasks::Detection> detections;
            const bool occluded = frame >= kGapStart && frame < kGapStart + kGapFrames;
            if (!occluded) {
                detections.push_back(makeDetection(20 + 5 * frame, 100, 60, 60, 0.9f, 0));
            }

            const std::vector<TrackedObject> tracks = tracker->update(detections);

            if (frame == kGapStart - 1 && tracks.size() == 1) {
                id_before_gap = tracks.front().track_id;
            }
            if (frame >= kGapStart + kGapFrames + 3) {
                for (const auto &track : tracks) {
                    ids_after_gap.insert(track.track_id);
                }
            }
        }

        CHECK_LABELED(id_before_gap > 0, entry.name);
        if (entry.recovers_after_gap) {
            CHECK_LABELED(ids_after_gap.size() == 1, entry.name);
            CHECK_LABELED(!ids_after_gap.empty() && *ids_after_gap.begin() == id_before_gap, entry.name);
        }
    }
}

// Two identical boxes move toward each other, pass, and separate. Identity must
// follow motion, not position: the id on the left-to-right object at the end
// must be the one it started with.
void testCrossingObjects() {
    const std::vector<NamedTracker> trackers = allTrackers();

    for (const auto &entry : trackers) {
        if (!entry.recovers_after_gap) {
            continue; // asserted for the two new trackers only
        }

        auto tracker = entry.factory(baseConfig(30));

        int rightward_id_start = -1;
        int rightward_id_end = -1;

        for (int frame = 0; frame < 32; ++frame) {
            const int rightward_x = 8 * frame;
            const int leftward_x = 250 - 8 * frame;
            const std::vector<neuriplo_tasks::Detection> detections = {makeDetection(rightward_x, 100, 40, 40, 0.9f, 0),
                                                                       makeDetection(leftward_x, 100, 40, 40, 0.9f, 0)};

            const std::vector<TrackedObject> tracks = tracker->update(detections);

            // Attribute output boxes to the two objects by proximity.
            const auto idFor = [&tracks](float expected_x) {
                int best_id = -1;
                float best_distance = 20.0f;
                for (const auto &track : tracks) {
                    const float distance = std::fabs(track.x - expected_x);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_id = track.track_id;
                    }
                }
                return best_id;
            };

            if (frame == 5) {
                rightward_id_start = idFor(static_cast<float>(rightward_x));
            }
            if (frame == 31) {
                rightward_id_end = idFor(static_cast<float>(rightward_x));
            }
        }

        CHECK_LABELED(rightward_id_start > 0, entry.name);
        CHECK_LABELED(rightward_id_start == rightward_id_end, entry.name);
    }
}

// Detections of a class that is not tracked must never produce a track.
void testClassFiltering() {
    for (const auto &entry : allTrackers()) {
        auto tracker = entry.factory(baseConfig(30)); // tracks class 0 only

        for (int frame = 0; frame < 12; ++frame) {
            const std::vector<neuriplo_tasks::Detection> detections = {
                makeDetection(20 + 5 * frame, 100, 60, 60, 0.9f, 7)};
            CHECK_LABELED(tracker->update(detections).empty(), entry.name);
        }
    }
}

// Empty frames and zero-area boxes must be survivable and must not invent tracks.
void testDegenerateInput() {
    for (const auto &entry : allTrackers()) {
        auto tracker = entry.factory(baseConfig(30));

        for (int frame = 0; frame < 5; ++frame) {
            // The braced empty list is ambiguous now that update() is
            // overloaded on the detection type, so name it.
            CHECK_LABELED(tracker->update(std::vector<neuriplo_tasks::Detection>{}).empty(), entry.name);
        }

        for (int frame = 0; frame < 5; ++frame) {
            const std::vector<neuriplo_tasks::Detection> detections = {makeDetection(50, 50, 0, 0, 0.9f, 0)};
            const std::vector<TrackedObject> tracks = tracker->update(detections);
            static_cast<void>(tracks); // no crash is the assertion for SORT/ByteTrack
            if (entry.recovers_after_gap) {
                CHECK_LABELED(tracks.empty(), entry.name);
            }
        }
    }
}

// The new wrappers carry the detection score through to the reported track.
void testConfidencePropagation() {
    for (const auto &entry : allTrackers()) {
        if (!entry.recovers_after_gap) {
            continue; // existing wrappers leave confidence unset; not changed here
        }

        auto tracker = entry.factory(baseConfig(30));

        for (int frame = 0; frame < 10; ++frame) {
            const std::vector<neuriplo_tasks::Detection> detections = {
                makeDetection(20 + 5 * frame, 100, 60, 60, 0.83f, 0)};
            const std::vector<TrackedObject> tracks = tracker->update(detections);
            if (frame >= 6) {
                CHECK_LABELED(tracks.size() == 1, entry.name);
                for (const auto &track : tracks) {
                    CHECK_LABELED(std::fabs(track.confidence - 0.83f) < 1e-5f, entry.name);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Mask-conditioned association
// ---------------------------------------------------------------------------

constexpr int kMaskFrameWidth = 240;
constexpr int kMaskFrameHeight = 200;

// A detection carrying a full-frame 0/255 mask, the way a segmentation model's
// postprocessor reports one.
neuriplo_tasks::InstanceSegmentation makeSegmentation(const cv::Rect &box, const cv::Rect &foreground, float confidence,
                                                      int class_id) {
    neuriplo_tasks::InstanceSegmentation segmentation;
    segmentation.bbox = neuriplo_tasks::BoundingBox(box.x, box.y, box.width, box.height);
    segmentation.class_confidence = confidence;
    segmentation.class_id = static_cast<float>(class_id);

    cv::Mat full = cv::Mat::zeros(kMaskFrameHeight, kMaskFrameWidth, CV_8U);
    if (foreground.width > 0 && foreground.height > 0) {
        full(foreground & cv::Rect(0, 0, kMaskFrameWidth, kMaskFrameHeight)).setTo(255);
    }
    segmentation.mask = neuriplo_tasks::fromCvMat(full);
    segmentation.mask_height = kMaskFrameHeight;
    segmentation.mask_width = kMaskFrameWidth;
    return segmentation;
}

std::vector<neuriplo_tasks::Detection>
sliceToDetections(const std::vector<neuriplo_tasks::InstanceSegmentation> &segmentations) {
    std::vector<neuriplo_tasks::Detection> detections;
    detections.reserve(segmentations.size());
    for (const auto &segmentation : segmentations) {
        detections.push_back(static_cast<const neuriplo_tasks::Detection &>(segmentation));
    }
    return detections;
}

// Track ids are handed out by a process-wide counter, so two tracker instances
// never produce the same numbers. Comparing the order in which ids first appear
// compares identity structure without depending on the counter.
std::vector<int> normalizeIds(const std::vector<std::vector<TrackedObject>> &frames) {
    std::map<int, int> assigned;
    std::vector<int> normalized;
    for (const auto &frame : frames) {
        for (const auto &track : frame) {
            const auto inserted = assigned.emplace(track.track_id, static_cast<int>(assigned.size()));
            normalized.push_back(inserted.first->second);
        }
    }
    return normalized;
}

// Feeding segmentations must behave exactly like feeding the detections inside
// them, for every tracker, when the mask cue is off.
void testSlicingIsTransparent() {
    for (const auto &entry : allTrackers()) {
        auto with_masks = entry.factory(baseConfig(30));
        auto without_masks = entry.factory(baseConfig(30));

        std::vector<std::vector<TrackedObject>> masked_frames;
        std::vector<std::vector<TrackedObject>> plain_frames;

        for (int frame = 0; frame < 16; ++frame) {
            const cv::Rect box(20 + 5 * frame, 60, 60, 60);
            const std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {
                makeSegmentation(box, box, 0.9f, 0)};

            masked_frames.push_back(with_masks->update(segmentations));
            plain_frames.push_back(without_masks->update(sliceToDetections(segmentations)));
        }

        CHECK_LABELED(normalizeIds(masked_frames) == normalizeIds(plain_frames), entry.name);

        for (size_t i = 0; i < masked_frames.size(); ++i) {
            CHECK_LABELED(masked_frames[i].size() == plain_frames[i].size(), entry.name);
            for (size_t j = 0; j < masked_frames[i].size() && j < plain_frames[i].size(); ++j) {
                CHECK_LABELED(std::fabs(masked_frames[i][j].x - plain_frames[i][j].x) < 1e-5f, entry.name);
                CHECK_LABELED(std::fabs(masked_frames[i][j].height - plain_frames[i][j].height) < 1e-5f, entry.name);
            }
        }
    }
}

// A constructed worst case for box-only association: two objects whose boxes sit
// almost on top of each other and swap places every frame, while their masks
// occupy different horizontal bands and never touch. Box overlap prefers the
// wrong pairing; mask overlap prefers the right one. The two objects have
// different heights so identity is visible in the output.
//
// Returns whether the tall object kept one identity for the whole sequence.
bool runMaskSwapSequence(const NamedTracker &entry, float mask_iou_weight) {
    TrackConfig config = baseConfig(30);
    config.mask_iou_weight = mask_iou_weight;
    auto tracker = entry.factory(config);

    int tall_id_first = -1;
    int tall_id_last = -1;

    for (int frame = 0; frame < 14; ++frame) {
        const bool swapped = (frame % 2) == 1;
        // 30px apart: the right pairing scores 0.33 box IoU, the wrong one
        // (same position, different height) scores 0.67, so box evidence points
        // the wrong way by more than OC-SORT's momentum term can correct.
        const int tall_x = swapped ? 130 : 100;
        const int short_x = swapped ? 100 : 130;

        // Tall object: 60x60 box, mask in the upper band.
        const cv::Rect tall_box(tall_x, 100, 60, 60);
        const cv::Rect tall_mask(tall_x, 100, 60, 20);
        // Short object: 60x40 box, mask in the lower band; the bands never overlap.
        const cv::Rect short_box(short_x, 100, 60, 40);
        const cv::Rect short_mask(short_x, 120, 60, 20);

        const std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {
            makeSegmentation(tall_box, tall_mask, 0.9f, 0), makeSegmentation(short_box, short_mask, 0.9f, 0)};

        const std::vector<TrackedObject> tracks = tracker->update(segmentations);

        for (const auto &track : tracks) {
            if (std::fabs(track.height - 60.0f) < 1e-3f) {
                if (tall_id_first < 0) {
                    tall_id_first = track.track_id;
                }
                tall_id_last = track.track_id;
            }
        }
    }

    return tall_id_first > 0 && tall_id_first == tall_id_last;
}

void testMaskCueDecides() {
    for (const auto &entry : allTrackers()) {
        if (!entry.recovers_after_gap) {
            continue; // the cue is implemented for OC-SORT and C-BIoU
        }

        // With masks the tall object keeps its identity...
        CHECK_LABELED(runMaskSwapSequence(entry, 1.0f), entry.name);

        // ...and without them it does not. This second assertion pins the case
        // as one the masks actually decide: if box-only association ever solves
        // it, the sequence must be made harder rather than this relaxed.
        CHECK_LABELED(!runMaskSwapSequence(entry, 0.0f), entry.name);
    }
}

// A segmented object that disappears for a few frames and comes back. A mask
// describes one frame and is never propagated, so the pre-gap mask sits at the
// old position and overlaps nothing. If it is still consulted on recovery it
// vetoes the box match and the identity is lost, meaning that switching the cue
// on would break the occlusion recovery these trackers exist for.
void testMaskDoesNotBlockRecovery() {
    constexpr int kGapStart = 8;
    constexpr int kGapFrames = 5;

    for (const auto &entry : allTrackers()) {
        if (!entry.recovers_after_gap) {
            continue;
        }

        TrackConfig config = baseConfig(30);
        config.mask_iou_weight = 1.0f; // the cue at full strength
        auto tracker = entry.factory(config);

        int id_before = -1;
        std::set<int> ids_after;
        for (int frame = 0; frame < 24; ++frame) {
            std::vector<neuriplo_tasks::InstanceSegmentation> segmentations;
            const bool occluded = frame >= kGapStart && frame < kGapStart + kGapFrames;
            if (!occluded) {
                const cv::Rect box(20 + 8 * frame, 100, 60, 60);
                segmentations.push_back(makeSegmentation(box, box, 0.9f, 0));
            }

            const std::vector<TrackedObject> tracks = tracker->update(segmentations);
            if (frame == kGapStart - 1 && tracks.size() == 1) {
                id_before = tracks.front().track_id;
            }
            if (frame >= kGapStart + kGapFrames + 3) {
                for (const auto &track : tracks) {
                    ids_after.insert(track.track_id);
                }
            }
        }

        CHECK_LABELED(id_before > 0, entry.name);
        CHECK_LABELED(ids_after.size() == 1, entry.name);
        CHECK_LABELED(!ids_after.empty() && *ids_after.begin() == id_before, entry.name);
    }
}

// A stream where only some detections carry masks must still track the rest.
void testPartialMasks() {
    for (const auto &entry : allTrackers()) {
        if (!entry.recovers_after_gap) {
            continue;
        }

        TrackConfig config = baseConfig(30);
        config.mask_iou_weight = 1.0f;
        auto tracker = entry.factory(config);

        std::set<int> ids;
        for (int frame = 0; frame < 14; ++frame) {
            const cv::Rect masked_box(20 + 5 * frame, 40, 50, 50);
            const cv::Rect bare_box(150, 120, 50, 50);

            std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {
                makeSegmentation(masked_box, masked_box, 0.9f, 0)};

            // Second object reports a box but no mask at all.
            neuriplo_tasks::InstanceSegmentation bare;
            bare.bbox = neuriplo_tasks::BoundingBox(bare_box.x, bare_box.y, bare_box.width, bare_box.height);
            bare.class_confidence = 0.9f;
            bare.class_id = 0.0f;
            segmentations.push_back(bare);

            const std::vector<TrackedObject> tracks = tracker->update(segmentations);
            if (frame >= 6) {
                CHECK_LABELED(tracks.size() == 2, entry.name);
                for (const auto &track : tracks) {
                    ids.insert(track.track_id);
                }
            }
        }

        CHECK_LABELED(ids.size() == 2, entry.name);
    }
}

// A track offered two candidates must not be pushed toward the lower-confidence
// one. The association cost adds a constant to keep it non-negative; if that
// constant is scaled by detection score, then with more detections than tracks
// (where the solver picks a subset of columns) a confident detection is
// penalised relative to a weak one.
void testConfidenceDoesNotPenalise() {
    TrackConfig config = baseConfig(30);
    config.ocsort_det_thresh = 0.1f; // so the weak candidate is not filtered out
    OCSortWrapper tracker(config);

    // Frame 0: one object establishes the track.
    const std::vector<TrackedObject> established =
        tracker.update(std::vector<neuriplo_tasks::Detection>{makeDetection(100, 100, 100, 100, 0.9f, 0)});
    CHECK(established.size() == 1);
    const int original_id = established.empty() ? -1 : established.front().track_id;

    // Frame 1: two candidates. The confident one also overlaps slightly better
    // (0.515 vs 0.493), so it is the correct answer on every count; only a
    // score-scaled offset could push the association to the weak one.
    const std::vector<neuriplo_tasks::Detection> candidates = {
        makeDetection(132, 100, 100, 100, 0.9f, 0), // IoU 0.515 with the track
        makeDetection(66, 100, 100, 100, 0.4f, 0),  // IoU 0.493 with the track
    };
    const std::vector<TrackedObject> tracks = tracker.update(candidates);

    // The unmatched candidate starts a track of its own, so the assertion has
    // to be about the original identity, not about any track being there.
    bool followed_confident = false;
    for (const auto &track : tracks) {
        if (track.track_id == original_id) {
            followed_confident = std::fabs(track.x - 132.0f) < 1e-3f;
        }
    }
    CHECK(followed_confident);
}

// A small object crossing a long gap. C-BIoU's motion model averages the
// displacement over its observation history; if that history does not record
// which frame each observation arrived on, the post-gap observation is treated
// as one frame after its predecessor and the estimated velocity is inflated by
// the length of the gap, so the prediction overshoots and the recovered
// identity is lost again. These parameters (small boxes, long gap, short
// history) are where the inflation exceeds the buffered matching radius.
void testRecoveryAfterLongGapIsStable() {
    constexpr int kGapStart = 6;
    constexpr int kGapFrames = 10;

    for (const auto &entry : allTrackers()) {
        if (!entry.recovers_after_gap) {
            continue;
        }

        TrackConfig config = baseConfig(40);
        config.cbiou_motion_n = 2;
        auto tracker = entry.factory(config);

        int id_before = -1;
        std::set<int> ids_after;
        for (int frame = 0; frame < 30; ++frame) {
            std::vector<neuriplo_tasks::Detection> detections;
            const bool occluded = frame >= kGapStart && frame < kGapStart + kGapFrames;
            if (!occluded) {
                detections.push_back(makeDetection(20 + 8 * frame, 100, 25, 25, 0.9f, 0));
            }

            const std::vector<TrackedObject> tracks = tracker->update(detections);
            if (frame == kGapStart - 1 && tracks.size() == 1) {
                id_before = tracks.front().track_id;
            }
            // Several frames after recovery, not just the first one: an
            // overshooting velocity shows up on the frames that follow.
            if (frame >= kGapStart + kGapFrames + 2) {
                for (const auto &track : tracks) {
                    ids_after.insert(track.track_id);
                }
            }
        }

        CHECK_LABELED(id_before > 0, entry.name);
        CHECK_LABELED(ids_after.size() == 1, entry.name);
        CHECK_LABELED(!ids_after.empty() && *ids_after.begin() == id_before, entry.name);
    }
}

} // namespace

int main() {
    testSteadyLinearMotion();
    testOcclusionGap();
    testCrossingObjects();
    testClassFiltering();
    testDegenerateInput();
    testConfidencePropagation();
    testConfidenceDoesNotPenalise();
    testRecoveryAfterLongGapIsStable();
    testSlicingIsTransparent();
    testMaskCueDecides();
    testMaskDoesNotBlockRecovery();
    testPartialMasks();
    return test_util::summary("trackers");
}
