#pragma once
#include "Detector.hpp"

class YoloV5 : public Detector {
public:
    YoloV5(const std::vector<std::string>& classNames,
           const std::string& modelConfiguration,
           const std::string& modelBinary,
           float confidenceThreshold = 0.5f,
           size_t network_width = 640,
           size_t network_height = 640,
           float scoreThreshold = 0.5f,
           float nmsThreshold = 0.45f);

private:
    float score_threshold_;
    float nms_threshold_;
    cv::dnn::Net net_;
    cv::Mat preprocessed_frame_;
    cv::Mat resized_frame_;
    
    cv::Mat preprocess_img(const cv::Mat& img);
    cv::Rect get_rect(const cv::Size& imgSize, const std::vector<float>& bbox) const;
    std::vector<Detection> run_detection(const cv::Mat& frame) override;
};