//
// Tracker-name canonicalization and AppConfig -> TrackConfig resolution.
//
#include "test_util.hpp"
#include "utils.hpp"

#include <string>

namespace {

AppConfig configFor(const std::string &algorithm) {
    AppConfig config;
    config.trackingAlgorithm = algorithm;
    config.classesToTrackIds = {0, 2};
    config.use_gpu = false;
    config.confidenceThreshold = 0.25f;
    config.batch_size = 1;
    config.displayOutput = false;
    return config;
}

void testCanonicalNames() {
    CHECK(canonicalTrackerName("SORT") == "SORT");
    CHECK(canonicalTrackerName("ByteTrack") == "ByteTrack");
    CHECK(canonicalTrackerName("BoTSORT") == "BoTSORT");

    // Both the paper spelling and the CLI spelling must resolve.
    CHECK(canonicalTrackerName("OCSORT") == "OCSORT");
    CHECK(canonicalTrackerName("OC-SORT") == "OCSORT");
    CHECK(canonicalTrackerName("oc_sort") == "OCSORT");
    CHECK(canonicalTrackerName("CBIoU") == "CBIoU");
    CHECK(canonicalTrackerName("C-BIoU") == "CBIoU");
    CHECK(canonicalTrackerName("cbiou") == "CBIoU");

    CHECK(canonicalTrackerName("DeepSORT").empty());
    CHECK(canonicalTrackerName("").empty());
}

// The existing trackers must keep the defaults they have today.
void testExistingTrackerDefaults() {
    const TrackConfig sort = makeTrackConfig(configFor("SORT"));
    CHECK_EQ(sort.max_age, 1);
    CHECK_EQ(sort.min_hits, 3);
    CHECK_NEAR(sort.iou_threshold, 0.3f, 1e-6f);

    const TrackConfig byte_track = makeTrackConfig(configFor("ByteTrack"));
    CHECK_EQ(byte_track.max_age, 1);
    CHECK_EQ(byte_track.track_buffer, 30);
    CHECK_NEAR(byte_track.track_thresh, 0.5f, 1e-6f);
    CHECK_NEAR(byte_track.high_thresh, 0.6f, 1e-6f);
    CHECK_NEAR(byte_track.match_thresh, 0.8f, 1e-6f);

    const TrackConfig botsort = makeTrackConfig(configFor("BoTSORT"));
    CHECK_EQ(botsort.max_age, 1);
}

// The new trackers need a lifetime long enough to recover a track across a gap.
void testNewTrackerDefaults() {
    for (const std::string &name : {std::string("OCSORT"), std::string("OC-SORT")}) {
        const TrackConfig config = makeTrackConfig(configFor(name));
        CHECK_LABELED(config.max_age == 30, name);
        CHECK_LABELED(config.min_hits == 3, name);
        CHECK_LABELED(std::fabs(config.iou_threshold - 0.3f) < 1e-6f, name);
        CHECK_LABELED(config.ocsort_delta_t == 3, name);
        CHECK_LABELED(std::fabs(config.ocsort_inertia - 0.2f) < 1e-6f, name);
        CHECK_LABELED(std::fabs(config.ocsort_det_thresh - 0.6f) < 1e-6f, name);
    }

    for (const std::string &name : {std::string("CBIoU"), std::string("C-BIoU")}) {
        const TrackConfig config = makeTrackConfig(configFor(name));
        CHECK_LABELED(config.max_age == 30, name);
        CHECK_LABELED(std::fabs(config.cbiou_b1 - 0.3f) < 1e-6f, name);
        CHECK_LABELED(std::fabs(config.cbiou_b2 - 0.5f) < 1e-6f, name);
        CHECK_LABELED(config.cbiou_motion_n == 5, name);
    }
}

// Anything given on the command line wins over the algorithm default.
void testOverridesWin() {
    AppConfig config = configFor("OCSORT");
    config.trackerOverrides.max_age = 5;
    config.trackerOverrides.min_hits = 1;
    config.trackerOverrides.iou_threshold = 0.55f;
    config.trackerOverrides.track_buffer = 60;
    config.trackerOverrides.track_thresh = 0.4f;
    config.trackerOverrides.high_thresh = 0.7f;
    config.trackerOverrides.match_thresh = 0.9f;
    config.trackerOverrides.ocsort_delta_t = 7;
    config.trackerOverrides.ocsort_inertia = 0.35f;
    config.trackerOverrides.ocsort_det_thresh = 0.15f;
    config.trackerOverrides.cbiou_b1 = 0.2f;
    config.trackerOverrides.cbiou_b2 = 0.8f;
    config.trackerOverrides.cbiou_motion_n = 9;

    const TrackConfig resolved = makeTrackConfig(config);
    CHECK_EQ(resolved.max_age, 5);
    CHECK_EQ(resolved.min_hits, 1);
    CHECK_NEAR(resolved.iou_threshold, 0.55f, 1e-6f);
    CHECK_EQ(resolved.track_buffer, 60);
    CHECK_NEAR(resolved.track_thresh, 0.4f, 1e-6f);
    CHECK_NEAR(resolved.high_thresh, 0.7f, 1e-6f);
    CHECK_NEAR(resolved.match_thresh, 0.9f, 1e-6f);
    CHECK_EQ(resolved.ocsort_delta_t, 7);
    CHECK_NEAR(resolved.ocsort_inertia, 0.35f, 1e-6f);
    CHECK_NEAR(resolved.ocsort_det_thresh, 0.15f, 1e-6f);
    CHECK_NEAR(resolved.cbiou_b1, 0.2f, 1e-6f);
    CHECK_NEAR(resolved.cbiou_b2, 0.8f, 1e-6f);
    CHECK_EQ(resolved.cbiou_motion_n, 9);
}

// Paths and class ids must survive the mapping.
void testPassThrough() {
    AppConfig config = configFor("BoTSORT");
    config.trackerConfigPath = "tracker.ini";
    config.gmcConfigPath = "gmc.ini";
    config.reidConfigPath = "reid.ini";
    config.reidOnnxPath = "reid.onnx";

    const TrackConfig resolved = makeTrackConfig(config);
    CHECK(resolved.tracker_config_path == "tracker.ini");
    CHECK(resolved.gmc_config_path == "gmc.ini");
    CHECK(resolved.reid_config_path == "reid.ini");
    CHECK(resolved.reid_onnx_path == "reid.onnx");
    CHECK(resolved.classes_to_track == std::set<int>({0, 2}));
}

// Out-of-range tuning values must be rejected with a message naming the flag,
// not passed into a tracker.
void testOverrideValidation() {
    TrackerOverrides valid;
    valid.max_age = 30;
    valid.min_hits = 3;
    valid.iou_threshold = 0.3f;
    valid.cbiou_b1 = 0.3f;
    valid.cbiou_b2 = 0.5f;
    CHECK(!validateTrackerOverrides(valid).has_value());

    const auto rejects = [](const TrackerOverrides &overrides, const std::string &flag) {
        const auto problem = validateTrackerOverrides(overrides);
        return problem && problem->find(flag) != std::string::npos;
    };

    TrackerOverrides negative_age;
    negative_age.max_age = -1;
    CHECK(rejects(negative_age, "--max_age"));

    TrackerOverrides zero_hits;
    zero_hits.min_hits = 0;
    CHECK(rejects(zero_hits, "--min_hits"));

    TrackerOverrides negative_iou;
    negative_iou.iou_threshold = -0.2f;
    CHECK(rejects(negative_iou, "--iou_threshold"));

    TrackerOverrides high_iou;
    high_iou.iou_threshold = 1.5f;
    CHECK(rejects(high_iou, "--iou_threshold"));

    TrackerOverrides bad_inertia;
    bad_inertia.ocsort_inertia = 2.0f;
    CHECK(rejects(bad_inertia, "--inertia"));

    TrackerOverrides bad_delta;
    bad_delta.ocsort_delta_t = 0;
    CHECK(rejects(bad_delta, "--delta_t"));

    TrackerOverrides bad_buffer;
    bad_buffer.cbiou_b1 = -0.1f;
    CHECK(rejects(bad_buffer, "--biou_b1"));

    TrackerOverrides bad_motion;
    bad_motion.cbiou_motion_n = 0;
    CHECK(rejects(bad_motion, "--motion_n"));

    // b1 > b2 is unusual but legal: the cascade still runs.
    TrackerOverrides reversed_buffers;
    reversed_buffers.cbiou_b1 = 0.6f;
    reversed_buffers.cbiou_b2 = 0.2f;
    CHECK(!validateTrackerOverrides(reversed_buffers).has_value());
}

} // namespace

int main() {
    testCanonicalNames();
    testExistingTrackerDefaults();
    testNewTrackerDefaults();
    testOverridesWin();
    testPassThrough();
    testOverrideValidation();
    return test_util::summary("track_config");
}
