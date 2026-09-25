#include "CommandLineParser.hpp"

#include "utils.hpp"

#include <filesystem>
#include <glog/logging.h>
#include <iostream>
#include <opencv2/core/utility.hpp>

const std::string CommandLineParser::params =
    "{ help h        |        | print help message }"
    "{ type          | yolov8 | detector model type }"
    "{ source        |        | input video file or stream URL }"
    "{ labels        |        | path to class labels file }"
    "{ weights       |        | path to model weights }"
    "{ tracker       | SORT   | tracking algorithm (SORT, ByteTrack, BoTSORT, OCSORT, CBIoU) }"
    "{ classes       |        | comma-separated list of classes to track }"
    "{ tracker_config| trackers/BoTSORT/config/tracker.ini | path to tracker config file }"
    "{ gmc_config    | trackers/BoTSORT/config/gmc.ini     | path to gmc config file }"
    "{ reid_config   | trackers/BoTSORT/config/reid.ini    | path to reid config file }"
    "{ reid_onnx     |        | path to reid onnx file }"
    // Tracker tuning; unset values fall back to the per-algorithm defaults
    "{ max_age       |        | frames a lost track survives (SORT 1, OCSORT/CBIoU 30) }"
    "{ min_hits      |        | detections before a track is reported (default 3) }"
    "{ iou_threshold |        | IoU association threshold (default 0.3) }"
    "{ track_buffer  |        | ByteTrack: lost-track buffer length (default 30) }"
    "{ track_thresh  |        | ByteTrack: first-stage detection threshold (default 0.5) }"
    "{ high_thresh   |        | ByteTrack: new-track detection threshold (default 0.6) }"
    "{ match_thresh  |        | ByteTrack: association threshold (default 0.8) }"
    "{ delta_t       |        | OCSORT: frame gap for velocity direction (default 3) }"
    "{ inertia       |        | OCSORT: weight of the direction-consistency cost (default 0.2) }"
    "{ det_thresh    |        | OCSORT: minimum detection score for a track (default 0.6) }"
    "{ biou_b1       |        | CBIoU: buffer scale, first matching round (default 0.3) }"
    "{ biou_b2       |        | CBIoU: buffer scale, second matching round (default 0.5) }"
    "{ motion_n      |        | CBIoU: observations averaged by the motion model (default 5) }"
    "{ mask_iou_weight |      | OCSORT/CBIoU: weight of mask overlap in association, 0-1 (default 0) }"
    "{ use-gpu       | false  | enable GPU support }"
    "{ min_confidence| 0.25   | minimum confidence threshold }"
    "{ batch         | 1      | batch size for inference }"
    "{ input_sizes   |        | input sizes for the model }"
    "{ output        |        | output video path (auto-generated only when no other output is requested) }"
    "{ display       | false  | display output video }"
    // Evaluation
    "{ mot_output      |      | write tracking results to this file, MOTChallenge format }"
    "{ save_detections |      | capture the detections fed to the tracker, same format, id=-1 }"
    "{ detections      |      | replay a captured detection file instead of running a detector }";

namespace {

// cv::CommandLineParser::has() reports whether a field currently holds a
// non-empty value, which is true for any field carrying a default -- so it
// cannot answer "did the user pass --type?" for a field whose default is
// "yolov8". The argument vector can, so it is consulted directly.
bool wasProvided(int argc, char *argv[], const std::string &name) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        for (const auto &prefix : {"--" + name, "-" + name}) {
            if (argument == prefix || argument.starts_with(prefix + "=")) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

AppConfig CommandLineParser::parseCommandLineArguments(int argc, char *argv[]) {
    cv::CommandLineParser parser(argc, argv, params);
    parser.about("Multi-Object Tracking Application");

    if (parser.has("help")) {
        printHelpMessage(parser);
        std::exit(0);
    }

    validateArguments(parser);

    AppConfig config;

    // Required parameters. On a replay, --type and --weights have no detector to
    // configure, so they are left empty unless the user actually passed them --
    // "yolov8" is the default of the --type field, not a request for a detector,
    // and validateEvaluationOptions() must be able to tell those apart.
    const bool replay = parser.has("detections");

    config.source = parser.get<std::string>("source");
    config.weights = parser.get<std::string>("weights");
    config.labelsPath = parser.get<std::string>("labels");
    config.detectorType =
        (replay && !wasProvided(argc, argv, "type")) ? std::string{} : parser.get<std::string>("type");
    config.trackingAlgorithm = parser.get<std::string>("tracker");

    // Parse classes to track
    std::string classesStr = parser.get<std::string>("classes");
    if (!classesStr.empty()) {
        config.classesToTrack = splitString(classesStr, ',');
    }

    // Tracker configuration
    config.trackerConfigPath = parser.get<std::string>("tracker_config");
    config.gmcConfigPath = parser.get<std::string>("gmc_config");
    config.reidConfigPath = parser.get<std::string>("reid_config");
    config.reidOnnxPath = parser.has("reid_onnx") ? parser.get<std::string>("reid_onnx") : "";

    // Tracker tuning parameters: only record what the user actually passed, so
    // makeTrackConfig() can tell "unset" from "set to the default value".
    parseTrackerOverrides(parser, config.trackerOverrides);

    if (const auto problem = validateTrackerOverrides(config.trackerOverrides)) {
        LOG(ERROR) << *problem;
        std::exit(1);
    }

    // Detection/inference settings
    config.use_gpu = parser.get<bool>("use-gpu");
    config.confidenceThreshold = parser.get<float>("min_confidence");
    config.batch_size = parser.get<int>("batch");

    // Input sizes if provided
    if (parser.has("input_sizes")) {
        std::string inputSizesStr = parser.get<std::string>("input_sizes");
        // Parse input sizes - implement parsing logic similar to object-detection-inference
        // For now, leave empty
    }

    // Output settings. An unset --output stays empty here; shouldWriteVideo()
    // decides whether a path is generated, because that decision depends on
    // the evaluation flags read just below.
    if (parser.has("output")) {
        config.outputPath = parser.get<std::string>("output");
    }
    config.displayOutput = parser.get<bool>("display");

    // Evaluation
    if (parser.has("mot_output")) {
        config.motOutputPath = parser.get<std::string>("mot_output");
    }
    if (parser.has("save_detections")) {
        config.saveDetectionsPath = parser.get<std::string>("save_detections");
    }
    if (replay) {
        config.detectionsPath = parser.get<std::string>("detections");
    }

    if (const auto problem = validateEvaluationOptions(config)) {
        LOG(ERROR) << *problem;
        std::exit(1);
    }

    if (replay && !config.classesToTrack.empty()) {
        LOG(WARNING) << "--classes is ignored on a replay: a detection file is captured after class filtering, "
                        "so it already contains exactly the classes that were tracked.";
    }

    if (!parser.has("output") && shouldWriteVideo(config)) {
        config.outputPath = generateOutputPath(config.source);
    }

    return config;
}

void CommandLineParser::parseTrackerOverrides(const cv::CommandLineParser &parser, TrackerOverrides &overrides) {
    const auto readInt = [&parser](const char *key, std::optional<int> &target) {
        if (parser.has(key)) {
            target = parser.get<int>(key);
        }
    };
    const auto readFloat = [&parser](const char *key, std::optional<float> &target) {
        if (parser.has(key)) {
            target = parser.get<float>(key);
        }
    };

    readInt("max_age", overrides.max_age);
    readInt("min_hits", overrides.min_hits);
    readFloat("iou_threshold", overrides.iou_threshold);

    readInt("track_buffer", overrides.track_buffer);
    readFloat("track_thresh", overrides.track_thresh);
    readFloat("high_thresh", overrides.high_thresh);
    readFloat("match_thresh", overrides.match_thresh);

    readInt("delta_t", overrides.ocsort_delta_t);
    readFloat("inertia", overrides.ocsort_inertia);
    readFloat("det_thresh", overrides.ocsort_det_thresh);

    readFloat("biou_b1", overrides.cbiou_b1);
    readFloat("biou_b2", overrides.cbiou_b2);
    readInt("motion_n", overrides.cbiou_motion_n);

    readFloat("mask_iou_weight", overrides.mask_iou_weight);
}

void CommandLineParser::printHelpMessage(const cv::CommandLineParser &parser) {
    std::cout << "\nMulti-Object Tracking Application\n" << std::endl;
    std::cout << "Usage: neuriplo-track [options]\n" << std::endl;
    parser.printMessage();
    std::cout << "\nExamples:\n";
    std::cout << "  ./neuriplo-track --source=video.mp4 --type=yolov8 --weights=model.onnx \\\n";
    std::cout << "    --labels=coco.names --tracker=ByteTrack --classes=person,car\n";
    std::cout << "  ./neuriplo-track --source=video.mp4 --type=yolov8 --weights=model.onnx \\\n";
    std::cout << "    --labels=coco.names --tracker=OCSORT --classes=person --max_age=45 --inertia=0.3\n";
    std::cout << "\nTrackers: SORT, ByteTrack, BoTSORT, OCSORT (or OC-SORT), CBIoU (or C-BIoU).\n";
    std::cout << "Tracker tuning flags left unset take the selected algorithm's default.\n" << std::endl;
}

void CommandLineParser::validateArguments(const cv::CommandLineParser &parser) {
    // A replay runs no detector, so it needs neither weights nor a labels file,
    // and its frames may come from the detection file rather than a video.
    // Everything a replay *does* require is checked here; the combinations that
    // are merely incoherent are left to validateEvaluationOptions().
    if (parser.has("detections")) {
        const std::string detections = parser.get<std::string>("detections");
        if (!std::filesystem::exists(detections)) {
            LOG(ERROR) << "Detection file not found: " << detections;
            std::exit(1);
        }
        if (parser.has("source") && !std::filesystem::exists(parser.get<std::string>("source"))) {
            LOG(ERROR) << "Video source not found: " << parser.get<std::string>("source");
            std::exit(1);
        }
        return;
    }

    if (!parser.has("source")) {
        LOG(ERROR) << "Source is required";
        printHelpMessage(parser);
        std::exit(1);
    }

    if (!parser.has("weights")) {
        LOG(ERROR) << "Weights path is required";
        printHelpMessage(parser);
        std::exit(1);
    }

    if (!parser.has("labels")) {
        LOG(ERROR) << "Labels file is required";
        printHelpMessage(parser);
        std::exit(1);
    }

    if (!std::filesystem::exists(parser.get<std::string>("weights"))) {
        LOG(ERROR) << "Weights file not found: " << parser.get<std::string>("weights");
        std::exit(1);
    }

    if (!std::filesystem::exists(parser.get<std::string>("labels"))) {
        LOG(ERROR) << "Labels file not found: " << parser.get<std::string>("labels");
        std::exit(1);
    }
}

std::set<int> CommandLineParser::mapClassesToIds(const std::vector<std::string> &classesToTrack,
                                                 const std::vector<std::string> &allClasses) {
    std::set<int> classIds;
    for (const auto &classToTrack : classesToTrack) {
        auto it = std::find(allClasses.begin(), allClasses.end(), classToTrack);
        if (it != allClasses.end()) {
            classIds.insert(std::distance(allClasses.begin(), it));
        } else {
            LOG(WARNING) << "Class '" << classToTrack << "' not found in labels file.";
        }
    }
    return classIds;
}
