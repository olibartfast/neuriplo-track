#include "CommandLineParser.hpp"
#include "utils.hpp"
#include <opencv2/core/utility.hpp>
#include <iostream>
#include <filesystem>
#include <glog/logging.h>

const std::string CommandLineParser::params = 
    "{ help h        |        | print help message }"
    "{ type          | yolov8 | detector model type }"
    "{ source        |        | input video file or stream URL }"
    "{ labels        |        | path to class labels file }"
    "{ weights       |        | path to model weights }"
    "{ tracker       | SORT   | tracking algorithm (SORT, ByteTrack, BoTSORT) }"
    "{ classes       |        | comma-separated list of classes to track }"
    "{ tracker_config| trackers/BoTSORT/config/tracker.ini | path to tracker config file }"
    "{ gmc_config    | trackers/BoTSORT/config/gmc.ini     | path to gmc config file }"
    "{ reid_config   | trackers/BoTSORT/config/reid.ini    | path to reid config file }"
    "{ reid_onnx     |        | path to reid onnx file }"
    "{ use-gpu       | false  | enable GPU support }"
    "{ min_confidence| 0.25   | minimum confidence threshold }"
    "{ batch         | 1      | batch size for inference }"
    "{ input_sizes   |        | input sizes for the model }"
    "{ output        |        | output video path (auto-generated if not specified) }"
    "{ display       | false  | display output video }";

AppConfig CommandLineParser::parseCommandLineArguments(int argc, char *argv[]) {
    cv::CommandLineParser parser(argc, argv, params);
    parser.about("Multi-Object Tracking Application");

    if (parser.has("help")) {
        printHelpMessage(parser);
        std::exit(0);
    }

    validateArguments(parser);

    AppConfig config;
    
    // Required parameters
    config.source = parser.get<std::string>("source");
    config.weights = parser.get<std::string>("weights");
    config.labelsPath = parser.get<std::string>("labels");
    config.detectorType = parser.get<std::string>("type");
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
    
    // Output settings
    if (parser.has("output")) {
        config.outputPath = parser.get<std::string>("output");
    } else {
        config.outputPath = generateOutputPath(config.source);
    }
    config.displayOutput = parser.get<bool>("display");
    
    return config;
}

void CommandLineParser::printHelpMessage(const cv::CommandLineParser& parser) {
    std::cout << "\nMulti-Object Tracking Application\n" << std::endl;
    std::cout << "Usage: vision-tracking [options]\n" << std::endl;
    parser.printMessage();
    std::cout << "\nExamples:\n";
    std::cout << "  ./vision-tracking --source=video.mp4 --type=yolov8 --weights=model.onnx \\\n";
    std::cout << "    --labels=coco.names --tracker=ByteTrack --classes=person,car\n" << std::endl;
}

void CommandLineParser::validateArguments(const cv::CommandLineParser& parser) {
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

std::set<int> CommandLineParser::mapClassesToIds(const std::vector<std::string>& classesToTrack, 
                                                  const std::vector<std::string>& allClasses) {
    std::set<int> classIds;
    for (const auto& classToTrack : classesToTrack) {
        auto it = std::find(allClasses.begin(), allClasses.end(), classToTrack);
        if (it != allClasses.end()) {
            classIds.insert(std::distance(allClasses.begin(), it));
        } else {
            LOG(WARNING) << "Class '" << classToTrack << "' not found in labels file.";
        }
    }
    return classIds;
}
