//
// SAM3 tracker behaviour against a scripted MaskPropagator.
//
// No model, no weights and no video are involved: the propagator is a
// ScriptedPropagator playing a per-frame script, so what is under test is the
// detector-prompted loop in Sam3Tracker plus the Sam3Wrapper plumbing —
// births, presence-gated output, occlusion loss, drift re-prompting, capacity,
// both update() overloads, class filtering and configuration.
//
#include "OCSortWrapper.hpp"
#include "Sam3Wrapper.hpp"
#include "ScriptedPropagator.hpp"
#include "test_util.hpp"

#include <cmath>
#include <memory>
#include <neuriplo/tasks/core/opencv_interop.hpp>
#include <opencv2/core.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kFrameWidth = 240;
constexpr int kFrameHeight = 200;

cv::Mat makeFrame() { return cv::Mat(kFrameHeight, kFrameWidth, CV_8UC3, cv::Scalar::all(0)); }

neuriplo_tasks::Detection makeDetection(float x, float y, float width, float height, float confidence, int class_id) {
    neuriplo_tasks::Detection detection;
    detection.bbox = neuriplo_tasks::BoundingBox(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width),
                                                 static_cast<int>(height));
    detection.class_confidence = confidence;
    detection.class_id = static_cast<float>(class_id);
    return detection;
}

// A detection carrying a full-frame 0/255 mask with the foreground band, the
// way a segmentation model's postprocessor reports one.
neuriplo_tasks::InstanceSegmentation makeSegmentation(const cv::Rect &box, const cv::Rect &foreground, float confidence,
                                                      int class_id) {
    neuriplo_tasks::InstanceSegmentation segmentation;
    segmentation.bbox = neuriplo_tasks::BoundingBox(box.x, box.y, box.width, box.height);
    segmentation.class_confidence = confidence;
    segmentation.class_id = static_cast<float>(class_id);

    cv::Mat full = cv::Mat::zeros(kFrameHeight, kFrameWidth, CV_8U);
    if (foreground.width > 0 && foreground.height > 0) {
        full(foreground & cv::Rect(0, 0, kFrameWidth, kFrameHeight)).setTo(255);
    }
    segmentation.mask = neuriplo_tasks::fromCvMat(full);
    segmentation.mask_height = kFrameHeight;
    segmentation.mask_width = kFrameWidth;
    return segmentation;
}

TrackConfig sam3Config() {
    TrackConfig config({0});
    config.max_age = 30;
    config.min_hits = 3;
    config.iou_threshold = 0.3f;
    config.sam_presence_thresh = 0.5f;
    config.sam_det_thresh = 0.5f;
    config.sam_reprompt_iou = 0.5f;
    config.sam_max_objects = 64;
    config.mask_iou_weight = 0.0f;
    return config;
}

// A wrapper plus a handle on the double behind it. The script must be set on
// the double before tracking starts.
struct Harness {
    std::unique_ptr<ScriptedPropagator> propagator;
    ScriptedPropagator *spy = nullptr;
    std::unique_ptr<Sam3Wrapper> tracker;

    explicit Harness(const TrackConfig &config, int capacity = 64) {
        propagator = std::make_unique<ScriptedPropagator>(capacity);
        spy = propagator.get();
        tracker = std::make_unique<Sam3Wrapper>(config, std::move(propagator));
    }
};

// A detection births exactly one object: one addObject with its box, and from
// the third match on (min_hits = 3) the same id is reported every frame with
// no further births.
void testBirthAndIdentity() {
    Harness harness(sam3Config());
    harness.spy->setScript({
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {25.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {30.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {35.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {40.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {45.0f, 100.0f, 60.0f, 60.0f}}},
    });
    const cv::Mat frame = makeFrame();

    int reported_id = -1;
    for (int step = 0; step < 6; ++step) {
        const std::vector<neuriplo_tasks::Detection> detections = {
            makeDetection(20.0f + 5.0f * step, 100, 60, 60, 0.9f, 0)};
        const std::vector<TrackedObject> tracks = harness.tracker->update(detections, frame);
        if (step < 2) {
            CHECK(tracks.empty());
        } else {
            CHECK(tracks.size() == 1);
            if (reported_id < 0) {
                reported_id = tracks.front().track_id;
            }
            CHECK(tracks.front().track_id == reported_id);
        }
    }

    CHECK(reported_id > 0);
    CHECK(harness.spy->addedBoxes().size() == 1);
    CHECK(std::fabs(harness.spy->addedBoxes().front().x - 20.0f) < 1e-5f);
    CHECK(harness.spy->reprompted().empty());
    CHECK(harness.spy->removedIds().empty());
}

// An object is not reported until it has min_hits detection matches.
void testMinHitsGatesOutput() {
    TrackConfig config = sam3Config();
    config.min_hits = 3;
    Harness harness(config);
    harness.spy->setScript({
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {25.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {30.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {35.0f, 100.0f, 60.0f, 60.0f}}},
    });
    const cv::Mat frame = makeFrame();

    for (int step = 0; step < 4; ++step) {
        const std::vector<neuriplo_tasks::Detection> detections = {
            makeDetection(20.0f + 5.0f * step, 100, 60, 60, 0.9f, 0)};
        const std::vector<TrackedObject> tracks = harness.tracker->update(detections, frame);
        if (step < 2) {
            CHECK(tracks.empty());
        } else {
            CHECK(tracks.size() == 1);
        }
    }
}

// The swap geometry from testMaskCueDecides: two objects teleport between two
// positions every frame, so a motion model pairs same-position boxes (wrong)
// while the propagator reports each id at its true position. SAM3 keeps both
// identities; OC-SORT on boxes alone does not, which pins the case as one
// propagation decides. The scripted masks carry the same band structure as the
// detections, so the run works with the cue on and off; attribution is by
// position, because the reported box is the propagated mask's box (here the
// band), not the detection box.
bool runSam3SwapSequence(float mask_iou_weight) {
    TrackConfig config = sam3Config();
    config.min_hits = 1;
    config.mask_iou_weight = mask_iou_weight;

    Harness harness(config);
    std::vector<std::vector<ScriptedObject>> script;
    for (int frame = 0; frame < 14; ++frame) {
        const bool swapped = (frame % 2) == 1;
        const float tall_x = swapped ? 130.0f : 100.0f;
        const float short_x = swapped ? 100.0f : 130.0f;
        // The propagator's masks carry the same band structure as the
        // detections below, at each object's true position.
        script.push_back({
            {0, {tall_x, 100.0f, 60.0f, 60.0f}, {tall_x, 100.0f, 60.0f, 20.0f}},
            {1, {short_x, 100.0f, 60.0f, 40.0f}, {short_x, 120.0f, 60.0f, 20.0f}},
        });
    }
    harness.spy->setScript(script);

    int tall_id_first = -1;
    int tall_id_last = -1;
    int full_frames = 0;
    const cv::Mat frame = makeFrame();
    for (int step = 0; step < 14; ++step) {
        const bool swapped = (step % 2) == 1;
        const int tall_x = swapped ? 130 : 100;
        const int short_x = swapped ? 100 : 130;
        const std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {
            makeSegmentation({tall_x, 100, 60, 60}, {tall_x, 100, 60, 20}, 0.9f, 0),
            makeSegmentation({short_x, 100, 60, 40}, {short_x, 120, 60, 20}, 0.9f, 0)};

        const std::vector<TrackedObject> tracks = harness.tracker->update(segmentations, frame);
        if (tracks.size() == 2) {
            ++full_frames;
            for (const auto &track : tracks) {
                if (std::fabs(track.x - static_cast<float>(tall_x)) < 1e-3f) {
                    if (tall_id_first < 0) {
                        tall_id_first = track.track_id;
                    }
                    tall_id_last = track.track_id;
                }
            }
        }
    }

    // Births report nothing, so the first of 14 frames is empty; the other 13
    // report both objects, and the one at the tall position is always id 1
    // (propagator id 0, births in detection order).
    return full_frames == 13 && tall_id_first == 1 && tall_id_first == tall_id_last;
}

void testPropagationHoldsIdentityWhereBoxesDoNot() {
    CHECK(runSam3SwapSequence(1.0f));
    CHECK(runSam3SwapSequence(0.0f));
}

// OC-SORT on boxes alone, mirroring baseConfig(30) from test_trackers.cpp.
TrackConfig baseConfigForComparison() {
    TrackConfig config({0});
    config.max_age = 30;
    config.min_hits = 3;
    config.iou_threshold = 0.3f;
    config.mask_iou_weight = 0.0f;
    return config;
}

// The same sequence through OC-SORT on boxes alone, recorded as the contrast:
// the tall object must NOT hold one identity there.
void testBoxOnlyLosesTheSwap() {
    OCSortWrapper ocsort(baseConfigForComparison());
    int tall_id_first = -1;
    int tall_id_last = -1;
    const cv::Mat frame = makeFrame();
    for (int step = 0; step < 14; ++step) {
        const bool swapped = (step % 2) == 1;
        const int tall_x = swapped ? 130 : 100;
        const int short_x = swapped ? 100 : 130;
        const std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {
            makeSegmentation({tall_x, 100, 60, 60}, {tall_x, 100, 60, 20}, 0.9f, 0),
            makeSegmentation({short_x, 100, 60, 40}, {short_x, 120, 60, 20}, 0.9f, 0)};

        for (const auto &track : ocsort.update(segmentations, frame)) {
            if (std::fabs(track.height - 60.0f) < 1e-3f) {
                if (tall_id_first < 0) {
                    tall_id_first = track.track_id;
                }
                tall_id_last = track.track_id;
            }
        }
    }
    CHECK(tall_id_first > 0);
    CHECK(tall_id_first != tall_id_last);
}

// Absence below the presence threshold suppresses output but keeps the id:
// a detection-free gap shorter than max_age resumes the same track with no
// removeObject and no new addObject.
void testAbsentResumes() {
    Harness harness(sam3Config());
    std::vector<std::vector<ScriptedObject>> script;
    for (int frame = 0; frame < 18; ++frame) {
        const bool gap = frame >= 8 && frame < 13;
        script.push_back({{0, {20.0f + 5.0f * frame, 100.0f, 60.0f, 60.0f}, gap ? 0.0f : 1.0f}});
    }
    harness.spy->setScript(script);
    const cv::Mat frame = makeFrame();

    int id_before = -1;
    int id_after = -1;
    for (int step = 0; step < 18; ++step) {
        std::vector<neuriplo_tasks::Detection> detections;
        if (step < 8 || step >= 13) {
            detections.push_back(makeDetection(20.0f + 5.0f * step, 100, 60, 60, 0.9f, 0));
        }
        const std::vector<TrackedObject> tracks = harness.tracker->update(detections, frame);
        if (step >= 8 && step < 13) {
            CHECK(tracks.empty());
        }
        if (step == 7 && tracks.size() == 1) {
            id_before = tracks.front().track_id;
        }
        if (step >= 16) {
            for (const auto &track : tracks) {
                id_after = track.track_id;
            }
        }
    }

    CHECK(id_before > 0);
    CHECK(id_after == id_before);
    CHECK(harness.spy->removedIds().empty());
    CHECK(harness.spy->addedBoxes().size() == 1);
}

// The same gap past max_age loses the object: exactly one removeObject, and
// the re-appearing detection births a new id.
void testAbsentTooLongIsLost() {
    TrackConfig config = sam3Config();
    config.max_age = 5;
    config.min_hits = 1;
    Harness harness(config);
    // Ids are handed out in birth order: the re-appearing detection is the
    // second birth, so the script names id 1 from frame 11 on.
    std::vector<std::vector<ScriptedObject>> script;
    for (int frame = 0; frame < 14; ++frame) {
        if (frame < 3) {
            script.push_back({{0, {20.0f + 5.0f * frame, 100.0f, 60.0f, 60.0f}, 1.0f}});
        } else if (frame < 11) {
            script.push_back({{0, {35.0f, 100.0f, 60.0f, 60.0f}, 0.0f}});
        } else {
            script.push_back({{1, {20.0f + 5.0f * frame, 100.0f, 60.0f, 60.0f}, 1.0f}});
        }
    }
    harness.spy->setScript(script);
    const cv::Mat frame = makeFrame();

    int id_before = -1;
    int id_after = -1;
    for (int step = 0; step < 14; ++step) {
        std::vector<neuriplo_tasks::Detection> detections;
        if (step < 3 || step >= 11) {
            detections.push_back(makeDetection(20.0f + 5.0f * step, 100, 60, 60, 0.9f, 0));
        }
        const std::vector<TrackedObject> tracks = harness.tracker->update(detections, frame);
        if (step == 2 && tracks.size() == 1) {
            id_before = tracks.front().track_id;
        }
        if (step == 13) {
            CHECK(tracks.size() == 1);
            id_after = tracks.front().track_id;
        }
    }

    CHECK(id_before > 0);
    CHECK(id_after > 0);
    CHECK(id_after != id_before);
    CHECK(harness.spy->removedIds().size() == 1);
    CHECK(harness.spy->removedIds().front() == 0);
    CHECK(harness.spy->addedBoxes().size() == 2);
}

// A masklet the detector never supports is removed all the same, even while
// SAM still reports it present: until removal it is still reported, then it
// stops. The negative case — a short unsupported stretch — keeps reporting
// with no removal.
void testUnsupportedMaskletsDie() {
    TrackConfig config = sam3Config();
    config.min_hits = 1;
    Harness harness(config);
    std::vector<std::vector<ScriptedObject>> script;
    for (int frame = 0; frame < 34; ++frame) {
        script.push_back({{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}});
    }
    harness.spy->setScript(script);
    const cv::Mat frame = makeFrame();

    bool reported_while_unsupported = false;
    bool reported_after_removal = false;
    for (int step = 0; step < 34; ++step) {
        std::vector<neuriplo_tasks::Detection> detections;
        if (step < 2) {
            detections.push_back(makeDetection(20, 100, 60, 60, 0.9f, 0));
        }
        const std::vector<TrackedObject> tracks = harness.tracker->update(detections, frame);
        if (step == 10) {
            reported_while_unsupported = tracks.size() == 1;
        }
        if (step == 33) {
            reported_after_removal = !tracks.empty();
        }
    }

    CHECK(reported_while_unsupported);
    CHECK(!reported_after_removal);
    CHECK(harness.spy->removedIds().size() == 1);

    // Short stretch: still reported, nothing removed.
    Harness short_harness(config);
    short_harness.spy->setScript({
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}, 1.0f}},
    });
    bool reported = false;
    for (int step = 0; step < 7; ++step) {
        std::vector<neuriplo_tasks::Detection> detections;
        if (step < 2) {
            detections.push_back(makeDetection(20, 100, 60, 60, 0.9f, 0));
        }
        if (!short_harness.tracker->update(detections, frame).empty()) {
            reported = true;
        }
    }
    CHECK(reported);
    CHECK(short_harness.spy->removedIds().empty());
}

// A matched pair whose propagated box has drifted from its detection feeds
// the detection back as a correction prompt for the same id; an agreeing pair
// re-prompts nothing.
void testDriftReprompts() {
    TrackConfig config = sam3Config();
    config.min_hits = 1;
    Harness harness(config);
    // The propagator sits still at (100, 100, 60, 60) the whole run.
    harness.spy->setScript({
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
    });
    const cv::Mat frame = makeFrame();

    // Shifted 30px: box IoU is 1/3, inside [iou_threshold, reprompt_iou).
    const std::vector<std::vector<neuriplo_tasks::Detection>> sequences = {
        {makeDetection(100, 100, 60, 60, 0.9f, 0)}, {makeDetection(100, 100, 60, 60, 0.9f, 0)},
        {makeDetection(100, 100, 60, 60, 0.9f, 0)}, {makeDetection(130, 100, 60, 60, 0.9f, 0)},
        {makeDetection(100, 100, 60, 60, 0.9f, 0)},
    };
    for (const auto &detections : sequences) {
        harness.tracker->update(detections, frame);
    }

    CHECK(harness.spy->reprompted().size() == 1);
    CHECK(harness.spy->reprompted().front().first == 0);
    CHECK(std::fabs(harness.spy->reprompted().front().second.x - 130.0f) < 1e-5f);

    // Control: full agreement the whole run re-prompts nothing.
    Harness aligned(config);
    aligned.spy->setScript({
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {100.0f, 100.0f, 60.0f, 60.0f}}},
    });
    for (int step = 0; step < 3; ++step) {
        const std::vector<neuriplo_tasks::Detection> detections = {makeDetection(100, 100, 60, 60, 0.9f, 0)};
        aligned.tracker->update(detections, frame);
    }
    CHECK(aligned.spy->reprompted().empty());
}

// Capacity bounds births: with sam_max_objects = 2 and three simultaneous new
// detections, two addObject calls happen, the third detection stays untracked,
// and the run continues without crashing.
void testCapacityRespected() {
    TrackConfig config = sam3Config();
    config.min_hits = 1;
    config.sam_max_objects = 2;
    Harness harness(config, 64);
    harness.spy->setScript({
        {},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}, {1, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}, {1, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}, {1, {100.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}, {1, {100.0f, 100.0f, 60.0f, 60.0f}}},
    });
    const cv::Mat frame = makeFrame();

    const std::vector<neuriplo_tasks::Detection> detections = {makeDetection(20, 100, 60, 60, 0.9f, 0),
                                                               makeDetection(100, 100, 60, 60, 0.9f, 0),
                                                               makeDetection(180, 100, 60, 60, 0.9f, 0)};
    for (int step = 0; step < 5; ++step) {
        const std::vector<TrackedObject> tracks = harness.tracker->update(detections, frame);
        if (step >= 1) {
            CHECK(tracks.size() == 2);
        }
    }
    CHECK(harness.spy->addedBoxes().size() == 2);
    CHECK(harness.spy->removedIds().empty());
}

// The Detection overload tracks, and the InstanceSegmentation overload with
// the cue off produces identical output: masks must not leak into the result
// when the weight is zero.
void testBothOverloads() {
    Harness plain(sam3Config());
    Harness masked(sam3Config());
    const std::vector<std::vector<ScriptedObject>> script = {
        {{0, {20.0f, 100.0f, 60.0f, 60.0f}}}, {{0, {25.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {30.0f, 100.0f, 60.0f, 60.0f}}}, {{0, {35.0f, 100.0f, 60.0f, 60.0f}}},
        {{0, {40.0f, 100.0f, 60.0f, 60.0f}}}, {{0, {45.0f, 100.0f, 60.0f, 60.0f}}},
    };
    plain.spy->setScript(script);
    masked.spy->setScript(script);
    const cv::Mat frame = makeFrame();

    for (int step = 0; step < 6; ++step) {
        const cv::Rect box(20 + 5 * step, 100, 60, 60);
        const std::vector<TrackedObject> plain_tracks = plain.tracker->update(
            std::vector<neuriplo_tasks::Detection>{makeDetection(20.0f + 5.0f * step, 100, 60, 60, 0.9f, 0)}, frame);
        const std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {makeSegmentation(box, box, 0.9f, 0)};
        const std::vector<TrackedObject> masked_tracks = masked.tracker->update(segmentations, frame);

        CHECK(plain_tracks.size() == masked_tracks.size());
        for (size_t i = 0; i < plain_tracks.size() && i < masked_tracks.size(); ++i) {
            CHECK(plain_tracks[i].track_id == masked_tracks[i].track_id);
            CHECK(std::fabs(plain_tracks[i].x - masked_tracks[i].x) < 1e-5f);
        }
    }
}

// Detections of untracked classes never reach addObject.
void testClassFilter() {
    Harness harness(sam3Config());
    harness.spy->setScript({{}, {}, {}});
    const cv::Mat frame = makeFrame();

    for (int step = 0; step < 3; ++step) {
        const std::vector<neuriplo_tasks::Detection> detections = {makeDetection(20, 100, 60, 60, 0.9f, 7)};
        CHECK(harness.tracker->update(detections, frame).empty());
    }
    CHECK(harness.spy->addedBoxes().empty());

    // An empty frame is an error on both overloads, not a silent no-op.
    bool threw_detections = false;
    try {
        harness.tracker->update(std::vector<neuriplo_tasks::Detection>{makeDetection(20, 100, 60, 60, 0.9f, 0)},
                                cv::Mat());
    } catch (const std::invalid_argument &) {
        threw_detections = true;
    }
    CHECK(threw_detections);

    bool threw_segmentations = false;
    try {
        const std::vector<neuriplo_tasks::InstanceSegmentation> segmentations = {
            makeSegmentation({20, 100, 60, 60}, {20, 100, 60, 60}, 0.9f, 0)};
        harness.tracker->update(segmentations, cv::Mat());
    } catch (const std::invalid_argument &) {
        threw_segmentations = true;
    }
    CHECK(threw_segmentations);
}

// Empty detection vectors on every frame produce no addObject and no output.
void testNoDetectionsNoObjects() {
    Harness harness(sam3Config());
    harness.spy->setScript({{}, {}, {}});
    const cv::Mat frame = makeFrame();

    for (int step = 0; step < 3; ++step) {
        CHECK(harness.tracker->update(std::vector<neuriplo_tasks::Detection>{}, frame).empty());
        CHECK(harness.tracker->update(std::vector<neuriplo_tasks::InstanceSegmentation>{}, frame).empty());
    }
    CHECK(harness.spy->addedBoxes().empty());
    CHECK(harness.spy->reprompted().empty());
    CHECK(harness.spy->removedIds().empty());
}

} // namespace

int main() {
    testBirthAndIdentity();
    testMinHitsGatesOutput();
    testPropagationHoldsIdentityWhereBoxesDoNot();
    testBoxOnlyLosesTheSwap();
    testAbsentResumes();
    testAbsentTooLongIsLost();
    testUnsupportedMaskletsDie();
    testDriftReprompts();
    testCapacityRespected();
    testBothOverloads();
    testClassFilter();
    testNoDetectionsNoObjects();
    return test_util::summary("sam3");
}
