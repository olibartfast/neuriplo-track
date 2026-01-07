# Code Examples and Implementation Details

This document contains detailed code examples and implementation specifics for the Multi-Object Tracking system.

## Table of Contents
1. [SORT Implementation](#sort-implementation)
2. [BoTSORT Implementation](#botsort-implementation)
3. [ByteTrack Implementation](#bytetrack-implementation)
4. [Wrapper Classes](#wrapper-classes)
5. [Configuration Examples](#configuration-examples)
6. [Integration Examples](#integration-examples)
7. [Performance Optimization](#performance-optimization)

---

## 1. SORT Implementation

### Core SORT Class Structure
```cpp
class Sort {
private:
    unsigned int max_age;
    int min_hits;
    double iouThreshold;
    std::vector<KalmanTracker> trackers;
    
public:
    Sort(unsigned int max_age = 30, int min_hits = 3, double iou_threshold = 0.3)
        : max_age(max_age), min_hits(min_hits), iouThreshold(iou_threshold) {}
    
    std::vector<TrackingBox> update(const std::vector<TrackingBox>& detections);
    
private:
    std::vector<std::vector<double>> getIouMatrix(
        const std::vector<cv::Rect_<float>>& predictions,
        const std::vector<cv::Rect_<float>>& detections
    );
    
    std::vector<std::pair<int, int>> associateDetectionsToTrackers(
        const std::vector<std::vector<double>>& iou_matrix,
        double iou_threshold
    );
};
```

### Kalman Filter State Vector
```cpp
// State vector: [x, y, s, r, dx, dy, ds]
// x, y: center coordinates
// s: scale (area) 
// r: aspect ratio (width/height)
// dx, dy, ds: velocities

class KalmanTracker {
private:
    cv::KalmanFilter kf;
    cv::Mat state;      // State vector
    cv::Mat measurement; // Measurement vector
    int time_since_update;
    int hit_streak;
    int age;
    int id;
    
public:
    KalmanTracker(cv::Rect_<float> initRect);
    void predict();
    void update(cv::Rect_<float> stateMat);
    cv::Rect_<float> get_state();
    
    // Getters for track management
    int getTimeSinceUpdate() const { return time_since_update; }
    int getHitStreak() const { return hit_streak; }
    int getAge() const { return age; }
    int getId() const { return id; }
};

// Convert bbox formats
cv::Rect_<float> tlwh_to_xysr(const cv::Rect_<float>& tlwh) {
    float center_x = tlwh.x + tlwh.width / 2.0f;
    float center_y = tlwh.y + tlwh.height / 2.0f;
    float scale = tlwh.width * tlwh.height;
    float ratio = tlwh.width / tlwh.height;
    return cv::Rect_<float>(center_x, center_y, scale, ratio);
}

cv::Rect_<float> xysr_to_tlwh(const cv::Rect_<float>& xysr) {
    float width = sqrt(xysr.width * xysr.height);
    float height = xysr.width / width;
    float x = xysr.x - width / 2.0f;
    float y = xysr.y - height / 2.0f;
    return cv::Rect_<float>(x, y, width, height);
}
```

### Hungarian Algorithm Integration
```cpp
#include "Hungarian.hpp"

std::vector<std::pair<int, int>> Sort::associateDetectionsToTrackers(
    const std::vector<std::vector<double>>& iou_matrix,
    double iou_threshold
) {
    std::vector<std::pair<int, int>> matched_indices;
    
    if (iou_matrix.empty()) {
        return matched_indices;
    }
    
    // Convert IoU to cost matrix (1 - IoU)
    std::vector<std::vector<double>> cost_matrix = iou_matrix;
    for (auto& row : cost_matrix) {
        for (auto& val : row) {
            val = 1.0 - val; // Convert similarity to cost
        }
    }
    
    // Apply Hungarian algorithm
    HungarianAlgorithm hungarian;
    std::vector<int> assignment;
    double cost = hungarian.Solve(cost_matrix, assignment);
    
    // Extract valid matches above IoU threshold
    for (size_t i = 0; i < assignment.size(); ++i) {
        if (assignment[i] >= 0) {
            double iou = iou_matrix[i][assignment[i]];
            if (iou >= iou_threshold) {
                matched_indices.emplace_back(i, assignment[i]);
            }
        }
    }
    
    return matched_indices;
}
```

---

## 2. BoTSORT Implementation
### Core BoTSORT Class
```cpp
namespace botsort {
    class BoTSORT {
    private:
        uint32_t _frame_id;
        std::vector<std::shared_ptr<Track>> tracked_tracks;
        std::vector<std::shared_ptr<Track>> lost_tracks;
        std::vector<std::shared_ptr<Track>> removed_tracks;
        
        // Configuration
        float high_conf_thresh;
        float low_conf_thresh;
        float match_thresh;
        int track_buffer;
        
        // Components
        std::shared_ptr<ReID> reid_model;
        std::unique_ptr<GlobalMotionCompensation> gmc;
        
    public:
        explicit BoTSORT(const std::string& tracker_config_path,
                        const std::string& gmc_config_path = "",
                        const std::string& reid_config_path = "",
                        const std::string& reid_onnx_model_path = "");
        
        std::vector<std::shared_ptr<Track>> track(
            const std::vector<Detection>& detections, 
            const cv::Mat& frame
        );
        
    private:
        FeatureVector _extract_features(const cv::Mat& frame,
                                      const cv::Rect_<float>& bbox_tlwh);
        
        std::vector<std::shared_ptr<Track>> _merge_track_lists(
            std::vector<std::shared_ptr<Track>>& tracks_list_a,
            std::vector<std::shared_ptr<Track>>& tracks_list_b
        );
        
        std::vector<std::shared_ptr<Track>> _remove_from_list(
            std::vector<std::shared_ptr<Track>>& tracks_list,
            const std::vector<std::shared_ptr<Track>>& tracks_to_remove
        );
    };
}
```

### Track Class Implementation
```cpp
namespace botsort {
    class Track {
    public:
        // Track state
        bool is_activated;
        int track_id;
        TrackState state;
        
        // Temporal information
        uint32_t frame_id, tracklet_len, start_frame;
        
        // Spatial information
        std::vector<float> det_tlwh;
        KFStateSpaceVec mean;
        KFStateSpaceMatrix covariance;
        
        // Appearance information
        std::shared_ptr<FeatureVector> curr_feat;
        std::unique_ptr<FeatureVector> smooth_feat;
        
        // Constructor
        Track(std::vector<float> tlwh, float score, uint8_t class_id,
              std::optional<FeatureVector> feat = std::nullopt,
              int feat_history_size = 50);
        
        // Core methods
        void predict();
        void update(const Detection& detection, uint32_t frame_id);
        void activate(uint32_t frame_id);
        void reactivate(const Detection& detection, uint32_t frame_id);
        
        // State management
        void mark_lost();
        void mark_long_lost();
        void mark_removed();
        
        // Getters
        std::vector<float> get_tlwh() const;
        float get_score() const;
        uint32_t end_frame() const;
        
    private:
        // Feature management
        void _update_features(const std::shared_ptr<FeatureVector>& feat);
        void _update_class_id(uint8_t class_id, float score);
        void _update_tracklet_tlwh_inplace();
        
        // Static utilities
        static void _populate_DetVec_xywh(DetVec& bbox_xywh,
                                        const std::vector<float>& tlwh);
        static int next_id();
        
        // Private members
        std::vector<float> _tlwh;
        std::vector<std::pair<uint8_t, float>> _class_hist;
        float _score;
        uint8_t _class_id;
        static constexpr float _alpha = 0.9;
        
        int _feat_history_size;
        std::deque<std::shared_ptr<FeatureVector>> _feat_history;
    };
}
```

### Feature Update Implementation
```cpp
void Track::_update_features(const std::shared_ptr<FeatureVector>& feat) {
    // L2 normalization
    *feat /= feat->norm();
    
    if (_feat_history.empty()) {
        curr_feat = feat;
        smooth_feat = std::make_unique<FeatureVector>(*curr_feat);
    } else {
        // Exponential moving average
        *smooth_feat = _alpha * (*smooth_feat) + (1 - _alpha) * (*feat);
    }
    
    // Maintain feature history
    if (_feat_history.size() == _feat_history_size) {
        _feat_history.pop_front();
    }
    _feat_history.push_back(curr_feat);
    
    // Normalize smooth features
    *smooth_feat /= smooth_feat->norm();
}
```

### Global Motion Compensation
```cpp
class GlobalMotionCompensation {
private:
    std::string method;
    int max_features;
    cv::Mat prev_frame;
    std::vector<cv::KeyPoint> prev_keypoints;
    cv::Mat prev_descriptors;
    
public:
    GlobalMotionCompensation(const std::string& config_path);
    
    cv::Mat estimateGlobalMotion(const cv::Mat& curr_frame) {
        if (prev_frame.empty()) {
            prev_frame = curr_frame.clone();
            return cv::Mat::eye(3, 3, CV_64F);
        }
        
        // Detect and match features
        std::vector<cv::KeyPoint> curr_keypoints;
        cv::Mat curr_descriptors;
        detectAndCompute(curr_frame, curr_keypoints, curr_descriptors);
        
        std::vector<cv::DMatch> matches = matchFeatures(prev_descriptors, curr_descriptors);
        
        if (matches.size() < 4) {
            return cv::Mat::eye(3, 3, CV_64F);
        }
        
        // Extract matched points
        std::vector<cv::Point2f> prev_pts, curr_pts;
        for (const auto& match : matches) {
            prev_pts.push_back(prev_keypoints[match.queryIdx].pt);
            curr_pts.push_back(curr_keypoints[match.trainIdx].pt);
        }
        
        // Estimate homography
        cv::Mat H = cv::findHomography(prev_pts, curr_pts, 
                                     cv::RANSAC, 3.0);
        
        // Update for next frame
        prev_frame = curr_frame.clone();
        prev_keypoints = curr_keypoints;
        prev_descriptors = curr_descriptors;
        
        return H.empty() ? cv::Mat::eye(3, 3, CV_64F) : H;
    }
    
private:
    void detectAndCompute(const cv::Mat& frame, 
                         std::vector<cv::KeyPoint>& keypoints,
                         cv::Mat& descriptors);
    
    std::vector<cv::DMatch> matchFeatures(const cv::Mat& desc1, 
                                        const cv::Mat& desc2);
};
```

### Re-ID Model Integration
```cpp
class ReID {
private:
    Ort::Session* session;
    Ort::Env env;
    std::vector<int64_t> input_shape;
    std::vector<int64_t> output_shape;
    
public:
    ReID(const std::string& model_path, const std::string& config_path) {
        // Initialize ONNX Runtime session
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        
        session = new Ort::Session(env, model_path.c_str(), session_options);
        
        // Get input/output shapes
        auto input_info = session->GetInputTypeInfo(0);
        auto input_tensor_info = input_info.GetTensorTypeAndShapeInfo();
        input_shape = input_tensor_info.GetShape();
        
        auto output_info = session->GetOutputTypeInfo(0);
        auto output_tensor_info = output_info.GetTensorTypeAndShapeInfo();
        output_shape = output_tensor_info.GetShape();
    }
    
    FeatureVector extract(const cv::Mat& crop) {
        // Preprocess image
        cv::Mat preprocessed = preprocess(crop);
        
        // Create input tensor
        std::vector<float> input_data = matToVector(preprocessed);
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_data.data(), input_data.size(),
            input_shape.data(), input_shape.size()
        );
        
        // Run inference
        const char* input_names[] = {"input"};
        const char* output_names[] = {"output"};
        
        auto output_tensors = session->Run(Ort::RunOptions{nullptr}, 
                                         input_names, &input_tensor, 1,
                                         output_names, 1);
        
        // Extract features
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        size_t feature_size = output_shape[1];
        
        FeatureVector features(feature_size);
        for (size_t i = 0; i < feature_size; ++i) {
            features[i] = output_data[i];
        }
        
        return features;
    }
    
private:
    cv::Mat preprocess(const cv::Mat& image) {
        cv::Mat resized, normalized, blob;
        
        // Resize to model input size
        cv::resize(image, resized, cv::Size(input_shape[3], input_shape[2]));
        
        // Normalize to [0, 1]
        resized.convertTo(normalized, CV_32F, 1.0/255.0);
        
        // Convert to blob (NCHW format)
        cv::dnn::blobFromImage(normalized, blob, 1.0, cv::Size(), cv::Scalar(), true, false);
        
        return blob;
    }
    
    std::vector<float> matToVector(const cv::Mat& mat) {
        return std::vector<float>(mat.begin<float>(), mat.end<float>());
    }
};
```

---

## 3. ByteTrack Implementation 

### Core ByteTracker Class
```cpp
class ByteTracker {
private:
    float high_thresh;
    float low_thresh;
    float match_thresh;
    int track_buffer;
    int frame_id;
    
    std::vector<STrack> tracked_stracks;
    std::vector<STrack> lost_stracks;
    std::vector<STrack> removed_stracks;
    
public:
    ByteTracker(float high_thresh = 0.6f, float low_thresh = 0.1f,
               float match_thresh = 0.8f, int track_buffer = 30)
        : high_thresh(high_thresh), low_thresh(low_thresh),
          match_thresh(match_thresh), track_buffer(track_buffer),
          frame_id(0) {}
    
    std::vector<STrack> update(const std::vector<Object>& objects) {
        frame_id++;
        
        // Separate detections by confidence
        std::vector<STrack> activated_stracks;
        std::vector<STrack> refind_stracks;
        
        std::vector<STrack> dets_high, dets_low;
        separateDetections(objects, dets_high, dets_low);
        
        // First association: high confidence detections
        std::vector<STrack> track_pool = jointStracks(tracked_stracks, lost_stracks);
        predictTracks(track_pool);
        
        auto [matches, unmatched_tracks, unmatched_dets] = 
            associateDetections(track_pool, dets_high, match_thresh);
        
        // Update matched tracks
        for (const auto& [track_idx, det_idx] : matches) {
            auto& track = track_pool[track_idx];
            auto& det = dets_high[det_idx];
            track.update(det, frame_id);
            activated_stracks.push_back(track);
        }
        
        // Second association: low confidence detections with unmatched tracks
        std::vector<STrack> r_tracked_stracks;
        for (int idx : unmatched_tracks) {
            if (track_pool[idx].state == TrackState::Tracked) {
                r_tracked_stracks.push_back(track_pool[idx]);
            }
        }
        
        auto [matches2, unmatched_tracks2, unmatched_dets2] = 
            associateDetections(r_tracked_stracks, dets_low, 0.5f);
        
        // Process second round matches
        for (const auto& [track_idx, det_idx] : matches2) {
            auto& track = r_tracked_stracks[track_idx];
            auto& det = dets_low[det_idx];
            track.update(det, frame_id);
            activated_stracks.push_back(track);
        }
        
        // Handle unmatched tracks and detections
        processUnmatchedTracks(unmatched_tracks2, r_tracked_stracks);
        initNewTracks(unmatched_dets, dets_high, activated_stracks);
        
        // Remove old tracks
        removeOldTracks();
        
        return activated_stracks;
    }
    
private:
    void separateDetections(const std::vector<Object>& objects,
                           std::vector<STrack>& high_conf,
                           std::vector<STrack>& low_conf);
    
    std::tuple<std::vector<std::pair<int, int>>, 
              std::vector<int>, 
              std::vector<int>>
    associateDetections(const std::vector<STrack>& tracks,
                       const std::vector<STrack>& detections,
                       float threshold);
};
```

### STrack (Single Track) Implementation
```cpp
class STrack {
public:
    enum TrackState { New = 0, Tracked, Lost, Removed };
    
    // Track properties
    int track_id;
    TrackState state;
    cv::Rect_<float> _tlwh;
    float score;
    int start_frame;
    int tracklet_len;
    bool is_activated;
    
    // Kalman filter
    KalmanFilter kalman_filter;
    cv::Mat mean, covariance;
    
    // Constructors
    STrack() : track_id(0), state(New), score(0), start_frame(0),
               tracklet_len(0), is_activated(false) {}
    
    STrack(const cv::Rect_<float>& tlwh, float score, int class_id = 0)
        : _tlwh(tlwh), score(score), state(New), start_frame(0),
          tracklet_len(0), is_activated(false) {
        
        // Initialize Kalman filter
        kalman_filter.init(tlwh);
        std::tie(mean, covariance) = kalman_filter.initiate(tlwh_to_xyah(tlwh));
    }
    
    // Core methods
    void predict() {
        if (state != Tracked && state != Lost) return;
        
        std::tie(mean, covariance) = kalman_filter.predict(mean, covariance);
    }
    
    void update(const STrack& new_track, int frame_id) {
        _tlwh = new_track._tlwh;
        score = new_track.score;
        
        std::tie(mean, covariance) = kalman_filter.update(
            mean, covariance, tlwh_to_xyah(_tlwh)
        );
        
        tracklet_len++;
        state = Tracked;
        is_activated = true;
    }
    
    void activate(int frame_id) {
        track_id = nextId();
        tracklet_len = 0;
        state = Tracked;
        if (frame_id == 1) {
            is_activated = true;
        }
        start_frame = frame_id;
    }
    
    void reActivate(const STrack& new_track, int frame_id) {
        std::tie(mean, covariance) = kalman_filter.update(
            mean, covariance, tlwh_to_xyah(new_track._tlwh)
        );
        
        tracklet_len = 0;
        state = Tracked;
        is_activated = true;
        score = new_track.score;
    }
    
    // State management
    void markLost() { state = Lost; }
    void markRemoved() { state = Removed; }
    
    // Getters
    cv::Rect_<float> tlwh() const {
        if (mean.empty()) return _tlwh;
        
        auto xyah = cv::Vec4f(mean.at<float>(0), mean.at<float>(1),
                             mean.at<float>(2), mean.at<float>(3));
        return xyah_to_tlwh(xyah);
    }
    
private:
    static int nextId() {
        static int _count = 0;
        return ++_count;
    }
    
    // Coordinate conversions
    cv::Vec4f tlwh_to_xyah(const cv::Rect_<float>& tlwh) const {
        return cv::Vec4f(tlwh.x + tlwh.width/2, tlwh.y + tlwh.height/2,
                        tlwh.width * tlwh.height, tlwh.width / tlwh.height);
    }
    
    cv::Rect_<float> xyah_to_tlwh(const cv::Vec4f& xyah) const {
        float w = sqrt(xyah[2] * xyah[3]);
        float h = xyah[2] / w;
        return cv::Rect_<float>(xyah[0] - w/2, xyah[1] - h/2, w, h);
    }
};
```

---

## 4. Wrapper Classes
### SortWrapper Implementation
```cpp
class SortWrapper : public BaseTracker {
private:
    Sort tracker;
    std::set<int> classes_to_track;
    
    // Configuration parameters
    double iou_threshold;
    int max_age;
    int min_hits;
    
public:
    explicit SortWrapper(const TrackConfig& config) 
        : classes_to_track(config.classes_to_track),
          iou_threshold(config.iou_threshold),
          max_age(config.max_age),
          min_hits(config.min_hits),
          tracker(max_age, min_hits, iou_threshold) {}
    
    std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame = cv::Mat()
    ) override {
        // Filter detections by class
        std::vector<Detection> filtered_detections;
        for (const auto& det : detections) {
            if (classes_to_track.empty() || 
                classes_to_track.count(det.class_id) > 0) {
                filtered_detections.push_back(det);
            }
        }
        
        // Convert to SORT format
        std::vector<TrackingBox> sort_detections;
        for (const auto& det : filtered_detections) {
            TrackingBox box;
            box.id = -1;  // Will be assigned by tracker
            box.box = det.bbox_tlwh;
            sort_detections.push_back(box);
        }
        
        // Run SORT tracking
        auto sort_results = tracker.update(sort_detections);
        
        // Convert back to TrackedObject format
        std::vector<TrackedObject> results;
        for (const auto& result : sort_results) {
            TrackedObject obj;
            obj.track_id = result.id;
            obj.x = result.box.x;
            obj.y = result.box.y;
            obj.width = result.box.width;
            obj.height = result.box.height;
            obj.confidence = 1.0f;  // SORT doesn't provide confidence
            results.push_back(obj);
        }
        
        return results;
    }
    
    void reset() override {
        tracker = Sort(max_age, min_hits, iou_threshold);
    }
    
    std::string getAlgorithmName() const override { return "SORT"; }
};
```

### BoTSORTWrapper Implementation
```cpp
class BoTSORTWrapper : public BaseTracker {
private:
    botsort::BoTSORT tracker;
    std::set<int> classes_to_track;
    bool initialized;
    
public:
    BoTSORTWrapper(const TrackConfig& config)
        : classes_to_track(config.classes_to_track),
          tracker(config.tracker_config_path, 
                 config.gmc_config_path, 
                 config.reid_config_path, 
                 config.reid_onnx_model_path),
          initialized(false)
    {
        validateConfiguration(config);
        initialized = true;
    }
    
    std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame = cv::Mat()
    ) override {
        if (!initialized) {
            throw std::runtime_error("BoTSORT not properly initialized");
        }
        
        if (frame.empty()) {
            throw std::invalid_argument("BoTSORT requires frame for Re-ID features");
        }
        
        // Filter detections by class
        std::vector<Detection> filtered_detections;
        for (const auto& det : detections) {
            if (classes_to_track.empty() || 
                classes_to_track.count(det.class_id) > 0) {
                filtered_detections.push_back(det);
            }
        }
        
        // Convert to BoTSORT format
        std::vector<botsort::Detection> botsort_detections;
        for (const auto& det : filtered_detections) {
            botsort::Detection botsort_det;
            botsort_det.bbox_tlwh = det.bbox_tlwh;
            botsort_det.confidence = det.confidence;
            botsort_det.class_id = static_cast<uint8_t>(det.class_id);
            botsort_detections.push_back(botsort_det);
        }
        
        // Run BoTSORT tracking
        auto botsort_results = tracker.track(botsort_detections, frame);
        
        // Convert results
        std::vector<TrackedObject> results;
        for (const auto& track : botsort_results) {
            TrackedObject obj;
            obj.track_id = track->track_id;
            
            const auto tlwh = track->get_tlwh();
            obj.x = tlwh[0];
            obj.y = tlwh[1];
            obj.width = tlwh[2];
            obj.height = tlwh[3];
            obj.confidence = track->get_score();
            
            results.push_back(obj);
        }
        
        return results;
    }
    
    bool isInitialized() const override { return initialized; }
    std::string getAlgorithmName() const override { return "BoTSORT"; }
    
private:
    void validateConfiguration(const TrackConfig& config) {
        if (config.reid_onnx_model_path.empty()) {
            throw std::invalid_argument("BoTSORT requires ReID model path");
        }
        
        if (!std::filesystem::exists(config.reid_onnx_model_path)) {
            throw std::runtime_error("ReID model file not found: " + config.reid_onnx_model_path);
        }
        
        // Validate other configuration files
        for (const auto& path : {config.tracker_config_path, 
                               config.gmc_config_path, 
                               config.reid_config_path}) {
            if (!path.empty() && !std::filesystem::exists(path)) {
                throw std::runtime_error("Configuration file not found: " + path);
            }
        }
    }
};
```

---

## 5. Configuration Examples

### JSON Configuration File
```json
{
    "tracking": {
        "algorithm": "BoTSORT",
        "classes": [0, 1, 2],
        "iou_threshold": 0.3,
        "max_age": 30,
        "min_hits": 3,
        "high_conf_thresh": 0.6,
        "low_conf_thresh": 0.1,
        "match_thresh": 0.8,
        "track_buffer": 30
    },
    "paths": {
        "tracker_config": "configs/tracker.ini",
        "gmc_config": "configs/gmc.ini",
        "reid_config": "configs/reid.ini",
        "reid_model": "models/reid_model.onnx"
    },
    "reid": {
        "input_size": [128, 256],
        "batch_size": 8,
        "feature_dim": 512
    }
}
```

### INI Configuration Files

#### tracker.ini
```ini
[tracker]
high_conf_thresh = 0.6
low_conf_thresh = 0.1
match_thresh = 0.8
track_buffer = 30
new_track_thresh = 0.7
track_high_thresh = 0.6
track_low_thresh = 0.1
max_time_lost = 30

[kalman]
# Kalman filter parameters
std_weight_position = 1./20
std_weight_velocity = 1./160
```

#### reid.ini
```ini
[reid]
model_path = models/reid_model.onnx
input_width = 128
input_height = 256
batch_size = 8
feature_dim = 512
normalize = true
mean = [0.485, 0.456, 0.406]
std = [0.229, 0.224, 0.225]
```

#### gmc.ini
```ini
[gmc]
method = orb
max_features = 1000
good_match_ratio = 0.15
ransac_threshold = 3.0
max_iterations = 2000
confidence = 0.99
```

### Configuration Loading Code
```cpp
class ConfigManager {
public:
    static TrackConfig loadFromJSON(const std::string& config_file) {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_file);
        }
        
        nlohmann::json config_json;
        file >> config_json;
        
        TrackConfig config;
        
        // Load tracking parameters
        if (config_json.contains("tracking")) {
            auto tracking = config_json["tracking"];
            
            // Load classes to track
            if (tracking.contains("classes")) {
                for (int cls : tracking["classes"]) {
                    config.classes_to_track.insert(cls);
                }
            }
            
            // Load thresholds
            config.iou_threshold = tracking.value("iou_threshold", 0.3);
            config.max_age = tracking.value("max_age", 30);
            config.min_hits = tracking.value("min_hits", 3);
            config.high_conf_thresh = tracking.value("high_conf_thresh", 0.6);
            config.low_conf_thresh = tracking.value("low_conf_thresh", 0.1);
            config.match_thresh = tracking.value("match_thresh", 0.8);
            config.track_buffer = tracking.value("track_buffer", 30);
        }
        
        // Load file paths
        if (config_json.contains("paths")) {
            auto paths = config_json["paths"];
            config.tracker_config_path = paths.value("tracker_config", "");
            config.gmc_config_path = paths.value("gmc_config", "");
            config.reid_config_path = paths.value("reid_config", "");
            config.reid_onnx_model_path = paths.value("reid_model", "");
        }
        
        return config;
    }
    
    static void saveToJSON(const TrackConfig& config, const std::string& config_file) {
        nlohmann::json config_json;
        
        // Save tracking parameters
        config_json["tracking"]["classes"] = std::vector<int>(
            config.classes_to_track.begin(), config.classes_to_track.end()
        );
        config_json["tracking"]["iou_threshold"] = config.iou_threshold;
        config_json["tracking"]["max_age"] = config.max_age;
        config_json["tracking"]["min_hits"] = config.min_hits;
        config_json["tracking"]["high_conf_thresh"] = config.high_conf_thresh;
        config_json["tracking"]["low_conf_thresh"] = config.low_conf_thresh;
        config_json["tracking"]["match_thresh"] = config.match_thresh;
        config_json["tracking"]["track_buffer"] = config.track_buffer;
        
        // Save paths
        config_json["paths"]["tracker_config"] = config.tracker_config_path;
        config_json["paths"]["gmc_config"] = config.gmc_config_path;
        config_json["paths"]["reid_config"] = config.reid_config_path;
        config_json["paths"]["reid_model"] = config.reid_onnx_model_path;
        
        // Write to file
        std::ofstream file(config_file);
        file << config_json.dump(4);
    }
};
```

---

## 6. Integration Examples {#integration-examples}

### Simple Video Processing
```cpp
#include "TrackerFactory.hpp"
#include "ConfigManager.hpp"
#include <opencv2/opencv.hpp>

int main(int argc, char** argv) {
    try {
        // Load configuration
        TrackConfig config = ConfigManager::loadFromJSON("config.json");
        
        // Create tracker
        auto tracker = TrackerFactory::createTracker("BoTSORT", config);
        
        // Open video
        cv::VideoCapture cap(argv[1]);
        if (!cap.isOpened()) {
            std::cerr << "Cannot open video: " << argv[1] << std::endl;
            return -1;
        }
        
        // Initialize detector (assuming YOLO)
        YOLODetector detector("models/yolo.onnx");
        
        cv::Mat frame;
        int frame_count = 0;
        
        while (cap.read(frame)) {
            // Object detection
            auto detections = detector.detect(frame);
            
            // Tracking update
            auto tracked_objects = tracker->update(detections, frame);
            
            // Visualization
            for (const auto& obj : tracked_objects) {
                cv::Rect rect(obj.x, obj.y, obj.width, obj.height);
                cv::rectangle(frame, rect, cv::Scalar(0, 255, 0), 2);
                
                std::string label = "ID: " + std::to_string(obj.track_id);
                cv::putText(frame, label, 
                           cv::Point(obj.x, obj.y - 10),
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                           cv::Scalar(0, 255, 0), 2);
            }
            
            // Display frame
            cv::imshow("Tracking", frame);
            if (cv::waitKey(1) == 27) break; // ESC key
            
            frame_count++;
        }
        
        std::cout << "Processed " << frame_count << " frames" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
```

### Multi-Camera Tracking System
```cpp
class MultiCameraTracker {
private:
    std::vector<std::unique_ptr<BaseTracker>> trackers;
    std::vector<cv::VideoCapture> cameras;
    std::mutex results_mutex;
    
public:
    MultiCameraTracker(const std::vector<std::string>& camera_sources,
                      const TrackConfig& config) {
        
        for (size_t i = 0; i < camera_sources.size(); ++i) {
            // Initialize camera
            cv::VideoCapture cap(camera_sources[i]);
            if (!cap.isOpened()) {
                throw std::runtime_error("Cannot open camera: " + camera_sources[i]);
            }
            cameras.push_back(std::move(cap));
            
            // Initialize tracker for each camera
            auto tracker = TrackerFactory::createTracker("BoTSORT", config);
            trackers.push_back(std::move(tracker));
        }
    }
    
    void processCamera(size_t camera_id, YOLODetector& detector) {
        cv::Mat frame;
        
        while (cameras[camera_id].read(frame)) {
            // Detection
            auto detections = detector.detect(frame);
            
            // Tracking
            auto tracked_objects = trackers[camera_id]->update(detections, frame);
            
            // Process results (store, visualize, etc.)
            processResults(camera_id, frame, tracked_objects);
        }
    }
    
    void run() {
        YOLODetector detector("models/yolo.onnx");
        std::vector<std::thread> threads;
        
        // Start thread for each camera
        for (size_t i = 0; i < cameras.size(); ++i) {
            threads.emplace_back(&MultiCameraTracker::processCamera, 
                               this, i, std::ref(detector));
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
private:
    void processResults(size_t camera_id, const cv::Mat& frame,
                       const std::vector<TrackedObject>& objects) {
        std::lock_guard<std::mutex> lock(results_mutex);
        
        // Visualize results
        cv::Mat display_frame = frame.clone();
        for (const auto& obj : objects) {
            cv::Rect rect(obj.x, obj.y, obj.width, obj.height);
            cv::rectangle(display_frame, rect, cv::Scalar(0, 255, 0), 2);
            
            std::string label = "Cam" + std::to_string(camera_id) + 
                              "_ID" + std::to_string(obj.track_id);
            cv::putText(display_frame, label, 
                       cv::Point(obj.x, obj.y - 10),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                       cv::Scalar(0, 255, 0), 2);
        }
        
        std::string window_name = "Camera " + std::to_string(camera_id);
        cv::imshow(window_name, display_frame);
        cv::waitKey(1);
    }
};
```

---

## 7. Performance Optimization {#performance-optimization}

### Threading and Parallelization
```cpp
class ParallelTracker {
private:
    std::unique_ptr<BaseTracker> tracker;
    std::queue<TrackingTask> task_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::vector<std::thread> worker_threads;
    std::atomic<bool> stop_flag;
    
    struct TrackingTask {
        std::vector<Detection> detections;
        cv::Mat frame;
        std::promise<std::vector<TrackedObject>> result_promise;
        
        TrackingTask(std::vector<Detection> dets, cv::Mat f)
            : detections(std::move(dets)), frame(std::move(f)) {}
    };
    
public:
    ParallelTracker(std::unique_ptr<BaseTracker> base_tracker, 
                   int num_threads = std::thread::hardware_concurrency())
        : tracker(std::move(base_tracker)), stop_flag(false) {
        
        // Start worker threads
        for (int i = 0; i < num_threads; ++i) {
            worker_threads.emplace_back(&ParallelTracker::workerLoop, this);
        }
    }
    
    ~ParallelTracker() {
        stop_flag = true;
        queue_cv.notify_all();
        
        for (auto& thread : worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    std::future<std::vector<TrackedObject>> updateAsync(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame
    ) {
        auto task = std::make_unique<TrackingTask>(detections, frame);
        auto future = task->result_promise.get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(std::move(*task));
        }
        queue_cv.notify_one();
        
        return future;
    }
    
private:
    void workerLoop() {
        while (!stop_flag) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this] { 
                return !task_queue.empty() || stop_flag; 
            });
            
            if (stop_flag) break;
            
            auto task = std::move(task_queue.front());
            task_queue.pop();
            lock.unlock();
            
            try {
                auto result = tracker->update(task.detections, task.frame);
                task.result_promise.set_value(result);
            } catch (const std::exception& e) {
                task.result_promise.set_exception(std::current_exception());
            }
        }
    }
};
```

### Memory Pool Implementation
```cpp
template<typename T>
class MemoryPool {
private:
    std::vector<std::unique_ptr<T>> pool;
    std::mutex mutex;
    size_t next_available = 0;
    
public:
    explicit MemoryPool(size_t initial_size = 100) {
        pool.reserve(initial_size);
        for (size_t i = 0; i < initial_size; ++i) {
            pool.push_back(std::make_unique<T>());
        }
    }
    
    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (next_available < pool.size()) {
            return std::move(pool[next_available++]);
        }
        
        return std::make_unique<T>();
    }
    
    void release(std::unique_ptr<T> obj) {
        if (!obj) return;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        if (next_available > 0) {
            pool[--next_available] = std::move(obj);
        }
    }
    
    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pool.size() - next_available;
    }
};

// Usage example
class OptimizedTrackingSystem {
private:
    MemoryPool<Detection> detection_pool;
    MemoryPool<TrackedObject> result_pool;
    std::unique_ptr<BaseTracker> tracker;
    
public:
    OptimizedTrackingSystem(const std::string& algorithm, 
                           const TrackConfig& config)
        : detection_pool(1000), result_pool(1000),
          tracker(TrackerFactory::createTracker(algorithm, config)) {}
    
    std::vector<TrackedObject> update(
        const std::vector<Detection>& detections,
        const cv::Mat& frame
    ) {
        // Use memory pool for internal processing
        std::vector<std::unique_ptr<Detection>> pooled_detections;
        
        for (const auto& det : detections) {
            auto pooled_det = detection_pool.acquire();
            *pooled_det = det;
            pooled_detections.push_back(std::move(pooled_det));
        }
        
        // Convert back for tracking
        std::vector<Detection> tracking_detections;
        for (const auto& det : pooled_detections) {
            tracking_detections.push_back(*det);
        }
        
        auto results = tracker->update(tracking_detections, frame);
        
        // Return objects to pool
        for (auto& det : pooled_detections) {
            detection_pool.release(std::move(det));
        }
        
        return results;
    }
};
```
