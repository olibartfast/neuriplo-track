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
#include <memory>
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
            CHECK_LABELED(tracker->update({}).empty(), entry.name);
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

} // namespace

int main() {
    testSteadyLinearMotion();
    testOcclusionGap();
    testCrossingObjects();
    testClassFiltering();
    testDegenerateInput();
    testConfidencePropagation();
    return test_util::summary("trackers");
}
