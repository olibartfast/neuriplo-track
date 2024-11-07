#include "YoloV5.hpp"
#include <stdexcept>
#include <algorithm>

// Constants for YOLO network
namespace {
    constexpr int DIMENSIONS = 85;  // numclasses + 5 (box coordinates + confidence)
    constexpr int NUM_ANCHORS = 25200;  // Default for 640x640 input
    constexpr float DEFAULT_PADDING_VALUE = 128.0f;
    constexpr float DEFAULT_SCALE = 1.0f / 255.0f;
}

YoloV5::YoloV5(
    const std::vector<std::string>& classNames,
    const std::string& modelConfiguration, 
    const std::string& modelBinary, 
    float confidenceThreshold,
    size_t network_width,
    size_t network_height,
    float scoreThreshold,
    float nmsThreshold
) : 
    Detector{classNames, modelConfiguration, modelBinary, confidenceThreshold,
            network_width, network_height},
    score_threshold_(scoreThreshold),
    nms_threshold_(nmsThreshold),
    preprocessed_frame_(network_height_, network_width_, CV_8UC3),
    resized_frame_(network_height_, network_width_, CV_8UC3)
{
    try {
        net_ = cv::dnn::readNetFromONNX(modelBinary);
        if (net_.empty()) {
            throw std::runtime_error("Failed to load network");
        }
        
        // Enable available backends and targets
        #ifdef CV_DNN_CUDA
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        #else
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        #endif

    } catch (const cv::Exception& e) {
        throw std::runtime_error("OpenCV error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Error initializing YOLOv5: " + std::string(e.what()));
    }
}

cv::Mat YoloV5::preprocess_img(const cv::Mat& img) {
    if (img.empty()) {
        throw std::runtime_error("Input image is empty");
    }

    float r_w = static_cast<float>(network_width_) / static_cast<float>(img.cols);
    float r_h = static_cast<float>(network_height_) / static_cast<float>(img.rows);
    float r = std::min(r_w, r_h);

    int new_width = static_cast<int>(img.cols * r);
    int new_height = static_cast<int>(img.rows * r);
    
    int x_offset = (network_width_ - new_width) / 2;
    int y_offset = (network_height_ - new_height) / 2;

    // Clear the preprocessing buffer with padding value
    preprocessed_frame_.setTo(cv::Scalar(DEFAULT_PADDING_VALUE, 
                                       DEFAULT_PADDING_VALUE, 
                                       DEFAULT_PADDING_VALUE));

    // Resize into the intermediate buffer
    cv::resize(img, resized_frame_(cv::Rect(0, 0, new_width, new_height)), 
               cv::Size(new_width, new_height), 0, 0, cv::INTER_LINEAR);

    // Copy the resized image into the padded frame
    cv::Mat roi = preprocessed_frame_(cv::Rect(x_offset, y_offset, new_width, new_height));
    resized_frame_(cv::Rect(0, 0, new_width, new_height)).copyTo(roi);

    return preprocessed_frame_;
}

cv::Rect YoloV5::get_rect(const cv::Size& imgSize, const std::vector<float>& bbox) const {
    if (bbox.size() != 4) {
        throw std::invalid_argument("Invalid bbox size");
    }

    float r_w = static_cast<float>(network_width_) / imgSize.width;
    float r_h = static_cast<float>(network_height_) / imgSize.height;
    float r = std::min(r_w, r_h);

    float x_offset = (network_width_ - imgSize.width * r) / 2;
    float y_offset = (network_height_ - imgSize.height * r) / 2;

    float x1 = (bbox[0] - bbox[2] / 2.0f - x_offset) / r;
    float x2 = (bbox[0] + bbox[2] / 2.0f - x_offset) / r;
    float y1 = (bbox[1] - bbox[3] / 2.0f - y_offset) / r;
    float y2 = (bbox[1] + bbox[3] / 2.0f - y_offset) / r;

    return cv::Rect(
        static_cast<int>(std::round(std::max(0.0f, x1))),
        static_cast<int>(std::round(std::max(0.0f, y1))),
        static_cast<int>(std::round(std::min(static_cast<float>(imgSize.width), x2) - x1)),
        static_cast<int>(std::round(std::min(static_cast<float>(imgSize.height), y2) - y1))
    );
}

std::vector<Detection> YoloV5::run_detection(const cv::Mat& frame) {
    if (frame.empty()) {
        throw std::runtime_error("Input frame is empty");
    }

    try {
        // Preprocess image
        cv::Mat preprocessed = preprocess_img(frame);
        cv::Mat blob;
        cv::dnn::blobFromImage(preprocessed, blob, DEFAULT_SCALE, 
                              cv::Size(), cv::Scalar(), true, false);

        // Run network
        net_.setInput(blob);
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, net_.getUnconnectedOutLayersNames());

        if (outputs.empty() || outputs[0].empty()) {
            throw std::runtime_error("Network produced no output");
        }

        // Reserve vectors with estimated capacity
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        classIds.reserve(NUM_ANCHORS);
        confidences.reserve(NUM_ANCHORS);
        boxes.reserve(NUM_ANCHORS);

        // Process detections
        float* data = reinterpret_cast<float*>(outputs[0].data);
        
        for (int i = 0; i < NUM_ANCHORS; ++i) {
            float confidence = data[4];
            if (confidence >= confidenceThreshold_) {
                float* classes_scores = data + 5;
                cv::Mat scores(1, classNames_.size(), CV_32FC1, classes_scores);
                
                double max_class_score;
                cv::Point class_id;
                cv::minMaxLoc(scores, nullptr, &max_class_score, nullptr, &class_id);

                if (max_class_score > score_threshold_) {
                    std::vector<float> bbox(data, data + 4);
                    cv::Rect box = get_rect(frame.size(), bbox);
                    
                    // Only add valid boxes
                    if (box.width > 0 && box.height > 0 && 
                        box.x >= 0 && box.y >= 0 && 
                        box.x + box.width <= frame.cols && 
                        box.y + box.height <= frame.rows) {
                        
                        boxes.push_back(box);
                        confidences.push_back(confidence);
                        classIds.push_back(class_id.x);
                    }
                }
            }
            data += DIMENSIONS;
        }

        // Perform NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, score_threshold_, 
                         nms_threshold_, indices);

        // Construct detection results
        std::vector<Detection> detections;
        detections.reserve(indices.size());

        for (int idx : indices) {
            Detection det;
            det.class_index = classIds[idx];
            det.confidence = confidences[idx];
            det.x = boxes[idx].x;
            det.y = boxes[idx].y;
            det.width = boxes[idx].width;
            det.height = boxes[idx].height;
            detections.push_back(det);
        }

        return detections;

    } catch (const cv::Exception& e) {
        throw std::runtime_error("OpenCV error during detection: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Error during detection: " + std::string(e.what()));
    }
}