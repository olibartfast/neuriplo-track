//
// The evaluation flag combinations: which runs are coherent, and when a video
// is written. Both rules live in pure functions precisely so they can be
// checked here without spawning a process or opening a video.
//
#include "test_util.hpp"
#include "utils.hpp"

#include <string>

namespace {

// A configuration for a normal detector-driven run.
AppConfig liveRun() {
    AppConfig config;
    config.source = "clip.mp4";
    config.weights = "model.onnx";
    config.labelsPath = "coco.names";
    config.detectorType = "yolov8";
    config.trackingAlgorithm = "SORT";
    config.use_gpu = false;
    config.confidenceThreshold = 0.25f;
    config.batch_size = 1;
    config.displayOutput = false;
    return config;
}

// A configuration for a replay: no detector, no weights, no source.
AppConfig replayRun() {
    AppConfig config;
    config.trackingAlgorithm = "OCSORT";
    config.detectionsPath = "det.txt";
    config.use_gpu = false;
    config.confidenceThreshold = 0.25f;
    config.batch_size = 1;
    config.displayOutput = false;
    return config;
}

void testReplayDetection() {
    CHECK(!liveRun().isReplay());
    CHECK(replayRun().isReplay());
}

void testLiveRunNeedsASource() {
    CHECK(!validateEvaluationOptions(liveRun()).has_value());

    AppConfig noSource = liveRun();
    noSource.source.clear();
    CHECK(validateEvaluationOptions(noSource).has_value());
}

void testReplayRejectsDetectorOptions() {
    // The bare replay is the one that must work: no model, no engine, no backend.
    CHECK(!validateEvaluationOptions(replayRun()).has_value());

    AppConfig withWeights = replayRun();
    withWeights.weights = "model.onnx";
    CHECK(validateEvaluationOptions(withWeights).has_value());

    AppConfig withType = replayRun();
    withType.detectorType = "yolov8";
    CHECK(validateEvaluationOptions(withType).has_value());

    // Capturing detections while replaying them has no detector to capture from.
    AppConfig withCapture = replayRun();
    withCapture.saveDetectionsPath = "det2.txt";
    CHECK(validateEvaluationOptions(withCapture).has_value());
}

void testReplayRejectsTheMaskCue() {
    // A MOT file carries boxes only, so the cue would be silently inert rather
    // than wrong -- which is exactly why it is rejected instead of ignored.
    AppConfig masked = replayRun();
    masked.trackerOverrides.mask_iou_weight = 0.5f;
    CHECK(validateEvaluationOptions(masked).has_value());

    // Explicitly disabling the cue is not a request for it.
    AppConfig zeroWeight = replayRun();
    zeroWeight.trackerOverrides.mask_iou_weight = 0.0f;
    CHECK(!validateEvaluationOptions(zeroWeight).has_value());
}

void testReplayNeedsASourceOnlyWhenItDrawsOrReids() {
    AppConfig rendering = replayRun();
    rendering.outputPath = "out.mp4";
    CHECK(validateEvaluationOptions(rendering).has_value());
    rendering.source = "clip.mp4";
    CHECK(!validateEvaluationOptions(rendering).has_value());

    AppConfig displaying = replayRun();
    displaying.displayOutput = true;
    CHECK(validateEvaluationOptions(displaying).has_value());
    displaying.source = "clip.mp4";
    CHECK(!validateEvaluationOptions(displaying).has_value());

    // BoTSORT reads the frame for Re-ID features; the rest ignore it.
    AppConfig botsort = replayRun();
    botsort.trackingAlgorithm = "BoTSORT";
    CHECK(validateEvaluationOptions(botsort).has_value());
    botsort.source = "clip.mp4";
    CHECK(!validateEvaluationOptions(botsort).has_value());

    // The paper spelling must be rejected for the same reason as the CLI one.
    AppConfig spelled = replayRun();
    spelled.trackingAlgorithm = "bot-sort";
    CHECK_EQ(canonicalTrackerName(spelled.trackingAlgorithm), std::string("BoTSORT"));
    CHECK(validateEvaluationOptions(spelled).has_value());
}

void testVideoIsWrittenOnlyWhenItWasAskedFor() {
    // An invocation that predates the evaluation flags keeps its auto-named video.
    CHECK(shouldWriteVideo(liveRun()));

    AppConfig explicitOutput = liveRun();
    explicitOutput.outputPath = "out.mp4";
    CHECK(shouldWriteVideo(explicitOutput));

    // Asking for a MOT file is asking for that, not for H.264 encoding.
    AppConfig motOnly = liveRun();
    motOnly.motOutputPath = "tracks.txt";
    CHECK(!shouldWriteVideo(motOnly));

    AppConfig captureOnly = liveRun();
    captureOnly.saveDetectionsPath = "det.txt";
    CHECK(!shouldWriteVideo(captureOnly));

    // ... unless a video was named explicitly alongside it.
    AppConfig both = liveRun();
    both.motOutputPath = "tracks.txt";
    both.outputPath = "out.mp4";
    CHECK(shouldWriteVideo(both));

    // A replay never invents a video path: it may have no source to render onto.
    CHECK(!shouldWriteVideo(replayRun()));

    AppConfig renderedReplay = replayRun();
    renderedReplay.source = "clip.mp4";
    renderedReplay.outputPath = "out.mp4";
    CHECK(shouldWriteVideo(renderedReplay));
}

} // namespace

int main() {
    testReplayDetection();
    testLiveRunNeedsASource();
    testReplayRejectsDetectorOptions();
    testReplayRejectsTheMaskCue();
    testReplayNeedsASourceOnlyWhenItDrawsOrReids();
    testVideoIsWrittenOnlyWhenItWasAskedFor();
    return test_util::summary("eval_options");
}
