#include "MultiObjectTrackingApp.hpp"
#include "CommandLineParser.hpp"
#include "utils.hpp"
#include "SortWrapper.hpp"
#include "ByteTrackWrapper.hpp"
#include "BoTSORTWrapper.hpp"
#include <glog/logging.h>
#include <filesystem>

MultiObjectTrackingApp::MultiObjectTrackingApp(const AppConfig& config)
    : config_(config) {
    try {
        setupLogging();

        LOG(INFO) << "Source: " << config_.source;
        LOG(INFO) << "Weights: " << config_.weights;
        LOG(INFO) << "Labels file: " << config_.labelsPath;
        LOG(INFO) << "Detector type: " << config_.detectorType;
        LOG(INFO) << "Tracker: " << config_.trackingAlgorithm;

        // Load class labels
        classes_ = readLabelNames(config_.labelsPath);
        
        // Map class names to IDs
        config_.classesToTrackIds = mapClassesToIds(config_.classesToTrack, classes_);
        
        if (config_.classesToTrackIds.empty()) {
            throw std::runtime_error("No valid classes to track");
        }

        // Setup inference engine
        engine_ = setup_inference_engine(config_.weights, config_.use_gpu, config_.batch_size, config_.input_sizes);
        if (!engine_) {
            throw std::runtime_error("Can't setup an inference engine for " + config_.weights);
        }

        const auto model_info = engine_->get_model_info();

        // Setup detector (DetectorSetup::createDetector takes only detectorType and model_info)
        detector_ = DetectorSetup::createDetector(config_.detectorType, model_info);
        if (!detector_) {
            throw std::runtime_error("Can't setup a detector: " + config_.detectorType);
        }

        // Setup tracker
        TrackConfig trackConfig(
            config_.classesToTrackIds,
            config_.trackerConfigPath,
            config_.gmcConfigPath,
            config_.reidConfigPath,
            config_.reidOnnxPath
        );
        
        tracker_ = createTracker(config_.trackingAlgorithm, trackConfig);
        if (!tracker_) {
            throw std::runtime_error("Can't setup tracker: " + config_.trackingAlgorithm);
        }

        // Initialize random colors for visualization
        cv::RNG rng(0xFFFFFFFF);
        colors_.resize(80);
        for (auto& color : colors_) {
            color = cv::Scalar(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));
        }

    } catch (const std::exception& e) {
        LOG(ERROR) << "Error during initialization: " << e.what();
        throw;
    }
}

void MultiObjectTrackingApp::run() {
    try {
        processVideo(config_.source);
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error during video processing: " << e.what();
        throw;
    }
}

void MultiObjectTrackingApp::setupLogging(const std::string& log_folder) {
    if (!std::filesystem::exists(log_folder)) {
        std::filesystem::create_directories(log_folder);
    }
    
    FLAGS_log_dir = log_folder;
    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging("vision-tracking");
}

void MultiObjectTrackingApp::processVideo(const std::string& source) {
    cv::VideoCapture cap(source);
    if (!cap.isOpened()) {
        throw std::runtime_error("Failed to open video source: " + source);
    }

    cv::VideoWriter videoWriter = setupVideoWriter(cap, config_.outputPath);
    if (!videoWriter.isOpened()) {
        throw std::runtime_error("Failed to open video writer: " + config_.outputPath);
    }

    LOG(INFO) << "Processing video...";
    LOG(INFO) << "Output will be saved to: " << config_.outputPath;

    cv::Mat frame;
    int frameCount = 0;
    
    while (cap.read(frame)) {
        // Run detection
        const auto input_blob = detector_->preprocess_image(frame);
        const auto [outputs, shapes] = engine_->get_infer_results(input_blob);
        std::vector<Detection> detections = detector_->postprocess(outputs, shapes, frame.size());

        // Run tracking
        auto tracks = tracker_->update(detections, frame);

        // Visualize results
        drawDetections(frame, detections);
        drawTracks(frame, tracks);

        // Write output
        videoWriter.write(frame);

        // Display if requested
        if (config_.displayOutput) {
            cv::imshow("Multi-Object Tracking", frame);
            if (cv::waitKey(1) == 27) { // ESC key
                break;
            }
        }

        frameCount++;
        if (frameCount % 30 == 0) {
            LOG(INFO) << "Processed " << frameCount << " frames";
        }
    }

    videoWriter.release();
    cap.release();
    
    if (config_.displayOutput) {
        cv::destroyAllWindows();
    }

    LOG(INFO) << "Processing complete. Total frames: " << frameCount;
    LOG(INFO) << "Output saved to: " << config_.outputPath;
}

void MultiObjectTrackingApp::drawDetections(cv::Mat& frame, const std::vector<Detection>& detections) {
    for (const auto& detection : detections) {
        // Only draw detections for tracked classes
        if (config_.classesToTrackIds.find(detection.label) != config_.classesToTrackIds.end()) {
            cv::rectangle(frame, detection.bbox, cv::Scalar(255, 255, 255), 2);
            
            std::string label = classes_[detection.label];
            cv::putText(frame, label,
                       cv::Point(detection.bbox.x, detection.bbox.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 2);
        }
    }
}

void MultiObjectTrackingApp::drawTracks(cv::Mat& frame, const std::vector<TrackedObject>& tracks) {
    for (const auto& track : tracks) {
        cv::Scalar color = colors_[track.track_id % colors_.size()];
        
        cv::Rect bbox(track.x, track.y, track.width, track.height);
        cv::rectangle(frame, bbox, color, 3);
        
        std::string trackLabel = "ID: " + std::to_string(track.track_id);
        cv::putText(frame, trackLabel,
                   cv::Point(track.x, track.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
    }
}

std::unique_ptr<BaseTracker> MultiObjectTrackingApp::createTracker(
    const std::string& trackingAlgorithm, 
    const TrackConfig& config) {
    
    if (trackingAlgorithm == "BoTSORT") {
        return std::make_unique<BoTSORTWrapper>(config);
    } else if (trackingAlgorithm == "SORT") {
        return std::make_unique<SortWrapper>(config);
    } else if (trackingAlgorithm == "ByteTrack") {
        return std::make_unique<ByteTrackWrapper>(config);
    }

    return nullptr;
}

std::set<int> MultiObjectTrackingApp::mapClassesToIds(
    const std::vector<std::string>& classesToTrack,
    const std::vector<std::string>& allClasses) {
    
    std::set<int> classIds;
    
    for (const auto& className : classesToTrack) {
        auto it = std::find(allClasses.begin(), allClasses.end(), className);
        if (it != allClasses.end()) {
            classIds.insert(static_cast<int>(std::distance(allClasses.begin(), it)));
        } else {
            LOG(WARNING) << "Class '" << className << "' not found in labels file";
        }
    }
    
    return classIds;
}
