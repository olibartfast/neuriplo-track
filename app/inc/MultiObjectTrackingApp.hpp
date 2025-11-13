#pragma once
#include "AppConfig.hpp"
#include "DetectorSetup.hpp"
#include "InferenceBackendSetup.hpp"
#include "BaseTracker.hpp"
#include "Detector.hpp"
#include <opencv2/opencv.hpp>
#include <memory>

class MultiObjectTrackingApp {
public:
    MultiObjectTrackingApp(const AppConfig& config);
    void run();
    
private:
    void setupLogging(const std::string& log_folder = "./logs");
    void processVideo(const std::string& source);
    void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections);
    void drawTracks(cv::Mat& frame, const std::vector<TrackedObject>& tracks);
    std::unique_ptr<BaseTracker> createTracker(const std::string& trackingAlgorithm, const TrackConfig& config);
    std::set<int> mapClassesToIds(const std::vector<std::string>& classesToTrack, const std::vector<std::string>& allClasses);
    
    AppConfig config_;
    std::unique_ptr<InferenceInterface> engine_;
    std::unique_ptr<Detector> detector_;
    std::unique_ptr<BaseTracker> tracker_;
    std::vector<std::string> classes_;
    std::vector<cv::Scalar> colors_;
};
