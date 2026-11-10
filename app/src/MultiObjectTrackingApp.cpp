#include "MultiObjectTrackingApp.hpp"

#include "BoTSORTWrapper.hpp"
#include "ByteTrackWrapper.hpp"
#include "CBIoUWrapper.hpp"
#include "CommandLineParser.hpp"
#include "MotFile.hpp"
#include "OCSortWrapper.hpp"
#include "SortWrapper.hpp"
#include "neuriplo/tasks/core/opencv_interop.hpp"
#include "neuriplo/tasks/core/task_config.hpp"
#include "neuriplo/tasks/core/task_factory.hpp"
#include "utils.hpp"

#include <filesystem>
#include <fstream>
#include <glog/logging.h>
#include <map>
#include <optional>

namespace {
// A frame with no detections still steps the tracker, so every tracker's age
// and lost-track bookkeeping advances exactly as it would on a live run.
const std::vector<neuriplo_tasks::Detection> kNoDetections{};
} // namespace

MultiObjectTrackingApp::MultiObjectTrackingApp(const AppConfig &config) : config_(config) {
    try {
        setupLogging();

        LOG(INFO) << "Source: " << config_.source;
        LOG(INFO) << "Weights: " << config_.weights;
        LOG(INFO) << "Labels file: " << config_.labelsPath;
        LOG(INFO) << "Detector type: " << config_.detectorType;
        LOG(INFO) << "Tracker: " << config_.trackingAlgorithm;

        if (config_.isReplay()) {
            LOG(INFO) << "Replaying detections from: " << config_.detectionsPath;
            LOG(INFO) << "No detector, inference engine or backend is constructed on this path.";

            // Every replayed record is a class this run tracks; see kReplayClassId.
            config_.classesToTrackIds = {kReplayClassId};

            TrackConfig replayConfig = makeTrackConfig(config_);
            replayConfig.classes_to_track = config_.classesToTrackIds;

            tracker_ = createTracker(config_.trackingAlgorithm, replayConfig);
            if (!tracker_) {
                throw std::runtime_error("Can't setup tracker: " + config_.trackingAlgorithm);
            }

            cv::RNG replayRng(0xFFFFFFFF);
            colors_.resize(80);
            for (auto &color : colors_) {
                color = cv::Scalar(replayRng.uniform(0, 256), replayRng.uniform(0, 256), replayRng.uniform(0, 256));
            }
            return;
        }

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
        if (config_.isReplay()) {
            replayDetections();
        } else {
            processVideo(config_.source);
        }
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

    // Rendering is optional. A run asked only for a MOT file or a detection
    // capture should not pay for H.264 encoding it never requested.
    const bool renderVideo = shouldWriteVideo(config_);

    cv::VideoWriter videoWriter;
    if (renderVideo) {
        videoWriter = setupVideoWriter(cap, config_.outputPath);
        if (!videoWriter.isOpened()) {
            throw std::runtime_error("Failed to open video writer: " + config_.outputPath);
        }
        LOG(INFO) << "Output will be saved to: " << config_.outputPath;
    } else {
        LOG(INFO) << "No video output requested; rendering is skipped.";
    }

    std::ofstream motStream;
    std::optional<MotWriter> motWriter;
    if (!config_.motOutputPath.empty()) {
        motStream.open(config_.motOutputPath, std::ios::trunc);
        if (!motStream) {
            throw std::runtime_error("Failed to open MOT output file: " + config_.motOutputPath);
        }
        motWriter.emplace(motStream);
        LOG(INFO) << "Tracking results (MOTChallenge format): " << config_.motOutputPath;
    }

    std::ofstream detectionStream;
    std::optional<MotWriter> detectionWriter;
    if (!config_.saveDetectionsPath.empty()) {
        detectionStream.open(config_.saveDetectionsPath, std::ios::trunc);
        if (!detectionStream) {
            throw std::runtime_error("Failed to open detection capture file: " + config_.saveDetectionsPath);
        }
        detectionWriter.emplace(detectionStream);
        LOG(INFO) << "Captured detections: " << config_.saveDetectionsPath;
    }

    LOG(INFO) << "Processing video...";

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
        bool any_mask = false;
        for (const auto &result : results) {
            if (std::holds_alternative<neuriplo_tasks::Detection>(result)) {
                const auto &detection = std::get<neuriplo_tasks::Detection>(result);
                detections.push_back(detection);

                // Carried along as a mask-less entry so that a frame holding
                // both kinds of result does not lose its plain detections when
                // the mask overload is chosen below.
                neuriplo_tasks::InstanceSegmentation carrier;
                static_cast<neuriplo_tasks::Detection &>(carrier) = detection;
                segmentations.push_back(std::move(carrier));
            } else if (std::holds_alternative<neuriplo_tasks::InstanceSegmentation>(result)) {
                const auto &segmentation = std::get<neuriplo_tasks::InstanceSegmentation>(result);
                segmentations.push_back(segmentation);
                detections.push_back(static_cast<const neuriplo_tasks::Detection &>(segmentation));
                any_mask = true;
            }
        }

        // Run tracking
        auto tracks = any_mask ? tracker_->update(segmentations, frame) : tracker_->update(detections, frame);

        // MOTChallenge frame numbers are 1-based; frameCount is still 0-based here.
        const int motFrame = frameCount + 1;

        if (motWriter) {
            motWriter->writeTracks(motFrame, tracks);
        }

        if (detectionWriter) {
            // Captured *after* class filtering, so the file holds exactly the
            // detections the tracker saw. That is what makes a replay of it
            // reproduce this run rather than approximate it -- and it is why a
            // replay needs no class column, which the format does not carry.
            for (const auto &detection : detections) {
                const int classId = static_cast<int>(detection.class_id);
                if (config_.classesToTrackIds.find(classId) == config_.classesToTrackIds.end()) {
                    continue;
                }
                detectionWriter->write(
                    MotRecord{motFrame, kMotDetectionId, static_cast<float>(detection.bbox.x),
                              static_cast<float>(detection.bbox.y), static_cast<float>(detection.bbox.width),
                              static_cast<float>(detection.bbox.height), detection.class_confidence});
            }
        }

        // Visualize results
        if (renderVideo || config_.displayOutput) {
            drawDetections(frame, detections);
            drawTracks(frame, tracks);
        }

        if (renderVideo) {
            videoWriter.write(frame);
        }

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

    if (renderVideo) {
        videoWriter.release();
    }
    cap.release();

    if (config_.displayOutput) {
        cv::destroyAllWindows();
    }

    LOG(INFO) << "Processing complete. Total frames: " << frameCount;
    if (renderVideo) {
        LOG(INFO) << "Output saved to: " << config_.outputPath;
    }
}

void MultiObjectTrackingApp::replayDetections() {
    const MotReadResult parsed = readMotFile(config_.detectionsPath);
    if (!parsed.ok()) {
        throw std::runtime_error(parsed.error);
    }

    // Grouped by frame, and ordered, so a file whose lines are shuffled replays
    // the same way as one written in order. std::map keeps the frames ascending
    // and lets a gap in the numbering stay a gap: a frame with no detections is
    // still a frame the tracker must be stepped through, or every track's age
    // would be wrong.
    std::map<int, std::vector<neuriplo_tasks::Detection>> byFrame;
    for (const MotRecord &record : parsed.records) {
        neuriplo_tasks::Detection detection;
        detection.class_id = static_cast<float>(kReplayClassId);
        detection.class_confidence = record.confidence;
        // Detection boxes are integral by the time they leave a detector
        // (neuriplo_tasks::BoundingBox is an integer rect), so this narrowing
        // recovers exactly what was captured.
        detection.bbox = neuriplo_tasks::BoundingBox(static_cast<int>(record.x), static_cast<int>(record.y),
                                                     static_cast<int>(record.width), static_cast<int>(record.height));
        byFrame[record.frame].push_back(detection);
    }

    const int lastFrame = byFrame.empty() ? 0 : byFrame.rbegin()->first;

    std::ofstream motStream;
    std::optional<MotWriter> motWriter;
    if (!config_.motOutputPath.empty()) {
        motStream.open(config_.motOutputPath, std::ios::trunc);
        if (!motStream) {
            throw std::runtime_error("Failed to open MOT output file: " + config_.motOutputPath);
        }
        motWriter.emplace(motStream);
        LOG(INFO) << "Tracking results (MOTChallenge format): " << config_.motOutputPath;
    }

    // Frames are opened when something is drawn -- and also when the tracker
    // itself reads them. BoTSORT extracts Re-ID features from the frame, so
    // handing it an empty Mat would not fail loudly, it would quietly track on
    // motion alone and report numbers that look like BoTSORT's but are not.
    // A replay of BoTSORT without --source is rejected in
    // validateEvaluationOptions() for the same reason.
    //
    // Every other tracker ignores the frame, which is the point of this path:
    // a replay whose only output is a MOT file needs no video at all and runs
    // on a machine with no model, no ONNX Runtime session and no GPU.
    const bool renderVideo = shouldWriteVideo(config_);
    const bool trackerNeedsFrame = canonicalTrackerName(config_.trackingAlgorithm) == "BoTSORT";
    const bool needFrames = renderVideo || config_.displayOutput || trackerNeedsFrame;

    cv::VideoCapture cap;
    cv::VideoWriter videoWriter;
    if (needFrames) {
        cap.open(config_.source);
        if (!cap.isOpened()) {
            throw std::runtime_error("Failed to open video source: " + config_.source);
        }
        if (renderVideo) {
            videoWriter = setupVideoWriter(cap, config_.outputPath);
            if (!videoWriter.isOpened()) {
                throw std::runtime_error("Failed to open video writer: " + config_.outputPath);
            }
        }
    }

    LOG(INFO) << "Replaying " << parsed.records.size() << " detections over " << lastFrame << " frames";

    cv::Mat frame;
    int frameCount = 0;

    for (int motFrame = 1; motFrame <= lastFrame; ++motFrame) {
        if (needFrames && !cap.read(frame)) {
            LOG(WARNING) << "Video source ran out at frame " << motFrame << " but the detection file continues to "
                         << lastFrame << "; stopping here.";
            break;
        }

        const auto found = byFrame.find(motFrame);
        const std::vector<neuriplo_tasks::Detection> &detections =
            (found != byFrame.end()) ? found->second : kNoDetections;

        // BoTSORT is the only tracker that reads the frame; everything else
        // ignores it, so an empty Mat is the correct argument when there is no
        // video, and a replay of BoTSORT without --source was already rejected.
        auto tracks = tracker_->update(detections, needFrames ? frame : cv::Mat());

        if (motWriter) {
            motWriter->writeTracks(motFrame, tracks);
        }

        if (renderVideo || config_.displayOutput) {
            drawDetections(frame, detections);
            drawTracks(frame, tracks);
            if (renderVideo) {
                videoWriter.write(frame);
            }
            if (config_.displayOutput) {
                cv::imshow("Multi-Object Tracking", frame);
                if (cv::waitKey(1) == 27) { // ESC key
                    break;
                }
            }
        }

        frameCount++;
        if (frameCount % 100 == 0) {
            LOG(INFO) << "Replayed " << frameCount << " frames";
        }
    }

    if (renderVideo) {
        videoWriter.release();
    }
    if (needFrames) {
        cap.release();
    }
    if (config_.displayOutput) {
        cv::destroyAllWindows();
    }

    LOG(INFO) << "Replay complete. Total frames: " << frameCount;
}

void MultiObjectTrackingApp::drawDetections(cv::Mat &frame, const std::vector<neuriplo_tasks::Detection> &detections) {
    for (const auto &detection : detections) {
        // Only draw detections for tracked classes
        if (config_.classesToTrackIds.find(static_cast<int>(detection.class_id)) != config_.classesToTrackIds.end()) {
            const cv::Rect bbox = neuriplo_tasks::toCvRect(detection.bbox);
            cv::rectangle(frame, bbox, cv::Scalar(255, 255, 255), 2);

            // A replay has no labels file, so classes_ is empty and the
            // synthetic replay class id indexes nothing. Draw the box alone
            // rather than reading past the end of the vector.
            const auto classIndex = static_cast<std::size_t>(detection.class_id);
            if (classIndex < classes_.size()) {
                cv::putText(frame, classes_[classIndex], cv::Point(bbox.x, bbox.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                            cv::Scalar(255, 255, 255), 2);
            }
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
