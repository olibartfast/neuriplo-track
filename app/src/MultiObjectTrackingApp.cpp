#include "MultiObjectTrackingApp.hpp"

#include "BoTSORTWrapper.hpp"
#include "ByteTrackWrapper.hpp"
#include "CBIoUWrapper.hpp"
#include "CommandLineParser.hpp"
#include "OCSortWrapper.hpp"
#include "SortWrapper.hpp"
#include "neuriplo/tasks/core/opencv_interop.hpp"
#include "neuriplo/tasks/core/task_config.hpp"
#include "neuriplo/tasks/core/task_factory.hpp"
#include "utils.hpp"

#include <filesystem>
#include <glog/logging.h>

MultiObjectTrackingApp::MultiObjectTrackingApp(const AppConfig &config) : config_(config) {
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

        // Adapter: Convert neuriplo::InferenceMetadata to neuriplo_tasks::ModelInfo
        auto metadata = engine_->get_inference_metadata();

        neuriplo_tasks::ModelInfo model_info;

        // Map inputs
        for (const auto &input : metadata.getInputs()) {
            model_info.addInput(input.name, input.shape, input.batch_size);
        }

        // Map outputs
        for (const auto &output : metadata.getOutputs()) {
            model_info.addOutput(output.name, output.shape, output.batch_size);
        }
        // Setup detector (use neuriplo_tasks::TaskFactory)
        neuriplo_tasks::TaskConfig task_config;
        task_config.confidence_threshold = config_.confidenceThreshold;
        detector_ = neuriplo_tasks::TaskFactory::createTaskInstance(config_.detectorType, model_info, task_config);
        if (!detector_) {
            throw std::runtime_error("Can't setup a detector: " + config_.detectorType);
        }

        // Setup tracker
        const TrackConfig trackConfig = makeTrackConfig(config_);

        tracker_ = createTracker(config_.trackingAlgorithm, trackConfig);
        if (!tracker_) {
            throw std::runtime_error("Can't setup tracker: " + config_.trackingAlgorithm);
        }

        // Initialize random colors for visualization
        cv::RNG rng(0xFFFFFFFF);
        colors_.resize(80);
        for (auto &color : colors_) {
            color = cv::Scalar(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));
        }

    } catch (const std::exception &e) {
        LOG(ERROR) << "Error during initialization: " << e.what();
        throw;
    }
}

void MultiObjectTrackingApp::run() {
    try {
        processVideo(config_.source);
    } catch (const std::exception &e) {
        LOG(ERROR) << "Error during video processing: " << e.what();
        throw;
    }
}

void MultiObjectTrackingApp::setupLogging(const std::string &log_folder) {
    if (!std::filesystem::exists(log_folder)) {
        std::filesystem::create_directories(log_folder);
    }

    FLAGS_log_dir = log_folder;
    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging("neuriplo-track");
}

void MultiObjectTrackingApp::processVideo(const std::string &source) {
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
        auto preprocessed_data = detector_->preprocess({neuriplo_tasks::vision::opencv::copyFromCvMat(frame)});
        const auto [outputs, shapes] = engine_->get_infer_results(preprocessed_data);

        std::vector<neuriplo_tasks::Tensor> tensors;
        for (size_t i = 0; i < outputs.size(); ++i) {
            neuriplo_tasks::Tensor tensor;
            tensor.shape = shapes[i];
            tensor.data = outputs[i];
            tensors.push_back(tensor);
        }

        auto results = detector_->postprocess(neuriplo_tasks::vision::Size(frame.cols, frame.rows), tensors);

        // Extract Detections. A segmentation model reports InstanceSegmentation
        // instead, which is a Detection carrying a mask: keep both, so drawing
        // and the tracker's detection path are unaffected while the masks stay
        // available as an association cue.
        std::vector<neuriplo_tasks::Detection> detections;
        std::vector<neuriplo_tasks::InstanceSegmentation> segmentations;
        for (const auto &result : results) {
            if (std::holds_alternative<neuriplo_tasks::Detection>(result)) {
                detections.push_back(std::get<neuriplo_tasks::Detection>(result));
            } else if (std::holds_alternative<neuriplo_tasks::InstanceSegmentation>(result)) {
                const auto &segmentation = std::get<neuriplo_tasks::InstanceSegmentation>(result);
                segmentations.push_back(segmentation);
                detections.push_back(static_cast<const neuriplo_tasks::Detection &>(segmentation));
            }
        }

        // Run tracking
        auto tracks =
            segmentations.empty() ? tracker_->update(detections, frame) : tracker_->update(segmentations, frame);

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

void MultiObjectTrackingApp::drawDetections(cv::Mat &frame, const std::vector<neuriplo_tasks::Detection> &detections) {
    for (const auto &detection : detections) {
        // Only draw detections for tracked classes
        if (config_.classesToTrackIds.find(static_cast<int>(detection.class_id)) != config_.classesToTrackIds.end()) {
            const cv::Rect bbox = neuriplo_tasks::toCvRect(detection.bbox);
            cv::rectangle(frame, bbox, cv::Scalar(255, 255, 255), 2);

            std::string label = classes_[static_cast<int>(detection.class_id)];
            cv::putText(frame, label, cv::Point(bbox.x, bbox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(255, 255, 255), 2);
        }
    }
}

void MultiObjectTrackingApp::drawTracks(cv::Mat &frame, const std::vector<TrackedObject> &tracks) {
    for (const auto &track : tracks) {
        cv::Scalar color = colors_[track.track_id % colors_.size()];

        cv::Rect bbox(track.x, track.y, track.width, track.height);
        cv::rectangle(frame, bbox, color, 3);

        std::string trackLabel = "ID: " + std::to_string(track.track_id);
        cv::putText(frame, trackLabel, cv::Point(track.x, track.y - 10), cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
    }
}

std::unique_ptr<BaseTracker> MultiObjectTrackingApp::createTracker(const std::string &trackingAlgorithm,
                                                                   const TrackConfig &config) {

    const std::string algorithm = canonicalTrackerName(trackingAlgorithm);

    if (algorithm == "BoTSORT") {
        return std::make_unique<BoTSORTWrapper>(config);
    } else if (algorithm == "SORT") {
        return std::make_unique<SortWrapper>(config);
    } else if (algorithm == "ByteTrack") {
        return std::make_unique<ByteTrackWrapper>(config);
    } else if (algorithm == "OCSORT") {
        return std::make_unique<OCSortWrapper>(config);
    } else if (algorithm == "CBIoU") {
        return std::make_unique<CBIoUWrapper>(config);
    }

    return nullptr;
}

std::set<int> MultiObjectTrackingApp::mapClassesToIds(const std::vector<std::string> &classesToTrack,
                                                      const std::vector<std::string> &allClasses) {

    std::set<int> classIds;

    for (const auto &className : classesToTrack) {
        auto it = std::find(allClasses.begin(), allClasses.end(), className);
        if (it != allClasses.end()) {
            classIds.insert(static_cast<int>(std::distance(allClasses.begin(), it)));
        } else {
            LOG(WARNING) << "Class '" << className << "' not found in labels file";
        }
    }

    return classIds;
}
