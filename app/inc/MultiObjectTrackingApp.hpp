#pragma once
#include "AppConfig.hpp"
#include "BaseTracker.hpp"
#include "InferenceBackendSetup.hpp"
#include "neuriplo/tasks/core/result_types.hpp"
#include "neuriplo/tasks/core/task_interface.hpp"
#include <memory>
#include <opencv2/opencv.hpp>

class MultiObjectTrackingApp {
public:
  MultiObjectTrackingApp(const AppConfig &config);
  void run();

private:
  void setupLogging(const std::string &log_folder = "./logs");
  void processVideo(const std::string &source);
  void drawDetections(cv::Mat &frame,
                      const std::vector<neuriplo_tasks::Detection> &detections);
  void drawTracks(cv::Mat &frame, const std::vector<TrackedObject> &tracks);
  std::unique_ptr<BaseTracker>
  createTracker(const std::string &trackingAlgorithm,
                const TrackConfig &config);
  std::set<int> mapClassesToIds(const std::vector<std::string> &classesToTrack,
                                const std::vector<std::string> &allClasses);

  AppConfig config_;
  std::unique_ptr<InferenceInterface> engine_;
  std::unique_ptr<neuriplo_tasks::TaskInterface> detector_;
  std::unique_ptr<BaseTracker> tracker_;
  std::vector<std::string> classes_;
  std::vector<cv::Scalar> colors_;
};
