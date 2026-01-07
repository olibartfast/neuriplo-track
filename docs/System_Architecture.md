# C++ System Architecture Guide

This document details the C++ system architecture and design patterns used in the Multi-Object Tracking implementation.

## Table of Contents
1. [Design Patterns and Structure](#design-patterns)
2. [Wrapper Classes](#wrapper-classes)
3. [Configuration System](#configuration-system)
4. [Factory Pattern](#factory-pattern)
5. [Data Structures](#data-structures)
6. [Memory Management](#memory-management)
7. [Error Handling](#error-handling)
8. [Performance Optimization](#performance-optimization)

## Related Documentation
- **[Code Examples](Code_Examples.md)** - Complete implementation details, configuration files, and integration patterns
- **[Tracking Algorithms](Tracking_Algorithms.md)** - Algorithm concepts and comparison guide

---

## 1. Design Patterns and Structure 
The system uses a **Strategy Pattern** to allow interchangeability of tracking algorithms:

```cpp
// Base abstract class
class BaseTracker {
public:
    virtual ~BaseTracker() = default;
    virtual std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame = cv::Mat()
    ) = 0;
};
```

### Strategy Pattern Benefits

1. **Algorithm Interchangeability**: Switch between SORT, ByteTrack, and BoTSORT at runtime
2. **Extensibility**: Easy to add new tracking algorithms
3. **Testability**: Each algorithm can be tested independently
4. **Maintainability**: Changes to one algorithm don't affect others

### Class Hierarchy

```
BaseTracker (Abstract)
├── SortWrapper
├── ByteTrackWrapper
└── BoTSORTWrapper
```

### Interface Design Principles

```cpp
class BaseTracker {
public:
    // Pure virtual destructor for proper cleanup
    virtual ~BaseTracker() = default;
    
    // Main tracking interface - must be implemented by all trackers
    virtual std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame = cv::Mat()  // Optional frame for appearance-based trackers
    ) = 0;
    
    // Optional virtual methods with default implementations
    virtual void reset() { /* Default: no-op */ }
    virtual bool isInitialized() const { return true; }
    virtual std::string getAlgorithmName() const = 0;
};
```

---

## 2. Wrapper Classes 

Each tracking algorithm is wrapped in a specific class that implements the `BaseTracker` interface:

### Wrapper Class Design Principles

**Adapter Pattern Implementation**:
- **Format Conversion**: Each wrapper handles format translation between common interfaces and algorithm-specific formats
- **Class Filtering**: Configurable object class filtering based on tracking requirements
- **Error Handling**: Algorithm-specific error handling and validation
- **Performance Optimization**: Efficient data conversion and memory management

**Common Wrapper Responsibilities**:
- Input detection filtering and preprocessing
- Algorithm-specific format conversion (Detection ↔ AlgorithmDetection)
- Core algorithm execution and result processing
- Output format standardization (AlgorithmResult → TrackedObject)
- Configuration validation and initialization

**Implementation Patterns**:
- **RAII**: Resource management for algorithm instances and configurations
- **Exception Safety**: Proper error propagation and resource cleanup
- **Const Correctness**: Immutable operations where appropriate

### Advanced Wrapper Features

**BoTSORT Wrapper Specializations**:
- **Multi-dependency Management**: Handles ReID models, GMC configuration, and tracker parameters
- **Runtime Validation**: Validates model files, configuration paths, and initialization state
- **Frame Requirement Enforcement**: Ensures frame data availability for appearance-based tracking
- **Resource Lifecycle Management**: Proper initialization, validation, and cleanup sequences

**Validation Strategies**:
- **Pre-initialization**: Configuration file and model path validation
- **Runtime Checks**: Frame availability and tracker state verification
- **Graceful Degradation**: Fallback mechanisms for missing optional components
- **Error Context**: Detailed error messages with context information

**Performance Considerations**:
- **Lazy Initialization**: Delay expensive operations until first use
- **Memory Pre-allocation**: Reserve capacity for expected result sizes
- **Efficient Conversions**: Minimize data copying during format transformations

### ByteTrackWrapper Implementation

```cpp
class ByteTrackWrapper : public BaseTracker {
private:
    std::unique_ptr<ByteTracker> tracker;  // Pointer for dynamic allocation
    std::set<int> classes_to_track;
    
public:
    ByteTrackWrapper(const TrackConfig& config) 
        : classes_to_track(config.classes_to_track)
    {
        // Initialize ByteTracker with config parameters
        tracker = std::make_unique<ByteTracker>(
            config.high_conf_thresh,
            config.low_conf_thresh,
            config.match_thresh,
            config.track_buffer
        );
    }
    
    std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame = cv::Mat()
    ) override {
        auto filtered_detections = filterByClass(detections);
        auto bytetrack_detections = convertToByteTrackFormat(filtered_detections);
        
        auto bytetrack_results = tracker->update(bytetrack_detections);
        
        return convertToTrackedObjects(bytetrack_results);
    }
    
    std::string getAlgorithmName() const override { return "ByteTrack"; }
};
```

---

## 3. Configuration System 
### TrackConfig Structure

```cpp
struct TrackConfig {
    // Common parameters
    std::set<int> classes_to_track;
    
    // Algorithm-specific paths
    std::string tracker_config_path;
    std::string gmc_config_path;
    std::string reid_config_path;
    std::string reid_onnx_model_path;
    
    // SORT parameters
    double iou_threshold = 0.3;
    int max_age = 30;
    int min_hits = 3;
    
    // ByteTrack/BoTSORT parameters
    double high_conf_thresh = 0.6;
    double low_conf_thresh = 0.1;
    double match_thresh = 0.8;
    int track_buffer = 30;
    
    // Constructor with defaults
    TrackConfig(const std::set<int>& classes = {}, 
                const std::string& trackerPath = "", 
                const std::string& gmcPath = "", 
                const std::string& reidPath = "", 
                const std::string& onnxPath = "")
        : classes_to_track(classes), 
          tracker_config_path(trackerPath), 
          gmc_config_path(gmcPath), 
          reid_config_path(reidPath), 
          reid_onnx_model_path(onnxPath) {}
    
    // Validation method
    bool validate() const {
        if (classes_to_track.empty()) {
            std::cerr << "Warning: No classes specified for tracking" << std::endl;
        }
        
        // Check file existence for paths
        for (const auto& path : {tracker_config_path, gmc_config_path, reid_config_path}) {
            if (!path.empty() && !std::filesystem::exists(path)) {
                std::cerr << "Config file not found: " << path << std::endl;
                return false;
            }
        }
        
        return true;
    }
};
```

### Configuration Management Principles

**Hierarchical Configuration System**:
- **Multi-format Support**: JSON, INI, and programmatic configuration
- **Default Values**: Sensible defaults for all optional parameters
- **Validation Pipeline**: Multi-stage validation with detailed error reporting
- **Environment Integration**: Support for environment variable overrides

**Configuration Categories**:
- **Common Parameters**: Classes to track, basic thresholds, performance settings
- **Algorithm-specific**: Specialized parameters for each tracking algorithm
- **File Paths**: Model files, configuration files, output directories
- **Runtime Settings**: Logging levels, debug options, profiling flags

**Loading Strategies**:
- **Lazy Loading**: Load configuration sections as needed
- **Caching**: Cache parsed configurations to avoid repeated parsing
- **Hot Reloading**: Support for runtime configuration updates
- **Validation Caching**: Cache validation results for performance

---

## 4. Factory Pattern 

### Tracker Factory Implementation

```cpp
class TrackerFactory {
public:
    static std::unique_ptr<BaseTracker> createTracker(
        const std::string& algorithm, 
        const TrackConfig& config
    ) {
        // Convert to lowercase for case-insensitive comparison
        std::string algo_lower = algorithm;
        std::transform(algo_lower.begin(), algo_lower.end(), algo_lower.begin(), ::tolower);
        
        if (algo_lower == "sort") {
            return std::make_unique<SortWrapper>(config);
        } 
        else if (algo_lower == "bytetrack") {
            return std::make_unique<ByteTrackWrapper>(config);
        }
        else if (algo_lower == "botsort") {
            return std::make_unique<BoTSORTWrapper>(config);
        }
        else {
            throw std::invalid_argument("Unsupported tracking algorithm: " + algorithm);
        }
    }
    
    static std::vector<std::string> getSupportedAlgorithms() {
        return {"SORT", "ByteTrack", "BoTSORT"};
    }
    
    static bool isAlgorithmSupported(const std::string& algorithm) {
        auto supported = getSupportedAlgorithms();
        return std::find_if(supported.begin(), supported.end(),
            [&algorithm](const std::string& supported_algo) {
                return strcasecmp(algorithm.c_str(), supported_algo.c_str()) == 0;
            }) != supported.end();
    }
};
```

### Advanced Factory Patterns

**Registration-based Factory**:
- **Dynamic Registration**: Runtime registration of new tracker types
- **Template-based Creation**: Type-safe tracker instantiation
- **Extensibility**: Easy addition of new algorithms without factory modification
- **Plugin Architecture**: Support for dynamically loaded tracker modules

**Factory Features**:
- **Name Resolution**: Case-insensitive algorithm name matching
- **Configuration Validation**: Pre-creation configuration validation
- **Error Handling**: Detailed error messages for creation failures
- **Metadata Support**: Algorithm capabilities and requirements discovery

**Advanced Patterns**:
- **Abstract Factory**: Different factories for different tracker categories
- **Builder Pattern**: Step-by-step tracker configuration and creation
- **Singleton Registry**: Global tracker type registry with lifecycle management

---

## 5. Data Structures 
### Data Structure Design Principles

**Detection Structure**:
- **Standardized Format**: Consistent bounding box representation (TLWH)
- **Extensibility**: Optional fields for enhanced tracking (features, timestamps)
- **Validation**: Built-in validation methods for data integrity
- **Format Conversions**: Utility methods for different coordinate systems
- **Memory Efficiency**: Compact representation with optional extensions

**Design Considerations**:
- **Coordinate Systems**: Support for TLWH, XYWH, XYXY formats
- **Optional Features**: Appearance features for Re-ID algorithms
- **Temporal Information**: Timestamps for multi-frame analysis
- **Validation Logic**: Comprehensive validity checking methods
- **Performance**: Efficient conversion between formats

**TrackedObject Structure**:
- **Identity Management**: Unique track IDs with lifecycle tracking
- **Spatial Information**: Bounding box with confidence scoring
- **Temporal Metadata**: Age, hits, time since last update
- **State Management**: Tentative/Confirmed/Deleted state tracking
- **Trajectory History**: Optional path recording with memory management

**Advanced Features**:
- **Quality Metrics**: Hit ratio, track stability indicators
- **Prediction Support**: Motion vectors and uncertainty estimates  
- **Classification**: Object class with confidence scoring
- **Memory Management**: Automatic trajectory pruning and efficient storage

### Conversion Utilities

```cpp
class FormatConverter {
public:
    // Convert between different detection formats
    static std::vector<TrackingBox> detectionsToSortFormat(
        const std::vector<Detection>& detections
    ) {
        std::vector<TrackingBox> sort_boxes;
        sort_boxes.reserve(detections.size());
        
        for (const auto& det : detections) {
            TrackingBox box;
            box.id = -1;  // Will be assigned by tracker
            box.box = det.bbox_tlwh;
            sort_boxes.push_back(box);
        }
        
        return sort_boxes;
    }
    
    static std::vector<botsort::Detection> detectionsToBoTSORTFormat(
        const std::vector<Detection>& detections
    ) {
        std::vector<botsort::Detection> botsort_detections;
        botsort_detections.reserve(detections.size());
        
        for (const auto& det : detections) {
            botsort::Detection botsort_det;
            botsort_det.bbox_tlwh = cv::Rect_<float>(det.bbox_tlwh);
            botsort_det.confidence = det.confidence;
            botsort_det.class_id = static_cast<uint8_t>(det.class_id);
            
            botsort_detections.push_back(botsort_det);
        }
        
        return botsort_detections;
    }
    
    // Convert tracker results back to common format
    static std::vector<TrackedObject> sortResultsToTrackedObjects(
        const std::vector<TrackingBox>& sort_results
    ) {
        std::vector<TrackedObject> objects;
        objects.reserve(sort_results.size());
        
        for (const auto& result : sort_results) {
            TrackedObject obj;
            obj.track_id = result.id;
            obj.x = result.box.x;
            obj.y = result.box.y;
            obj.width = result.box.width;
            obj.height = result.box.height;
            obj.confidence = 1.0f;  // SORT doesn't provide confidence
            obj.state = TrackedObject::Confirmed;
            
            objects.push_back(obj);
        }
        
        return objects;
    }
};
```

---

## 6. Memory Management 
### RAII Principles

```cpp
class TrackingSystem {
private:
    std::unique_ptr<BaseTracker> tracker_;
    std::vector<Detection> detection_buffer_;
    std::vector<TrackedObject> result_buffer_;
    
public:
    TrackingSystem(const std::string& algorithm, const TrackConfig& config)
        : tracker_(TrackerFactory::createTracker(algorithm, config))
    {
        // Pre-allocate buffers to avoid repeated allocations
        detection_buffer_.reserve(1000);
        result_buffer_.reserve(1000);
    }
    
    // Move constructor and assignment
    TrackingSystem(TrackingSystem&& other) noexcept 
        : tracker_(std::move(other.tracker_)),
          detection_buffer_(std::move(other.detection_buffer_)),
          result_buffer_(std::move(other.result_buffer_)) {}
    
    TrackingSystem& operator=(TrackingSystem&& other) noexcept {
        if (this != &other) {
            tracker_ = std::move(other.tracker_);
            detection_buffer_ = std::move(other.detection_buffer_);
            result_buffer_ = std::move(other.result_buffer_);
        }
        return *this;
    }
    
    // Disable copy constructor and assignment
    TrackingSystem(const TrackingSystem&) = delete;
    TrackingSystem& operator=(const TrackingSystem&) = delete;
    
    std::vector<TrackedObject> update(const std::vector<Detection>& detections, 
                                     const cv::Mat& frame = cv::Mat()) {
        if (!tracker_) {
            throw std::runtime_error("Tracker not initialized");
        }
        
        return tracker_->update(detections, frame);
    }
};
```

### Memory Pool for Frequent Allocations

```cpp
template<typename T>
class ObjectPool {
private:
    std::queue<std::unique_ptr<T>> pool_;
    std::mutex mutex_;
    
public:
    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (pool_.empty()) {
            return std::make_unique<T>();
        }
        
        auto obj = std::move(pool_.front());
        pool_.pop();
        return obj;
    }
    
    void release(std::unique_ptr<T> obj) {
        if (obj) {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push(std::move(obj));
        }
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }
};

// Usage example for detection objects
class DetectionPool {
private:
    static ObjectPool<Detection> pool_;
    
public:
    static std::unique_ptr<Detection> getDetection() {
        return pool_.acquire();
    }
    
    static void returnDetection(std::unique_ptr<Detection> detection) {
        // Reset detection state before returning to pool
        detection->confidence = 0.0f;
        detection->class_id = -1;
        detection->features.clear();
        pool_.release(std::move(detection));
    }
};
```

---

## 7. Error Handling 
### Exception Hierarchy

```cpp
// Base exception for tracking-related errors
class TrackingException : public std::exception {
private:
    std::string message_;
    
public:
    explicit TrackingException(const std::string& message) 
        : message_(message) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
};

// Specific exception types
class TrackerInitializationException : public TrackingException {
public:
    explicit TrackerInitializationException(const std::string& message)
        : TrackingException("Tracker initialization failed: " + message) {}
};

class ConfigurationException : public TrackingException {
public:
    explicit ConfigurationException(const std::string& message)
        : TrackingException("Configuration error: " + message) {}
};

class ModelLoadException : public TrackingException {
public:
    explicit ModelLoadException(const std::string& message)
        : TrackingException("Model loading failed: " + message) {}
};
```

### Error Handling Strategies

```cpp
class RobustTracker {
private:
    std::unique_ptr<BaseTracker> tracker_;
    std::string algorithm_name_;
    TrackConfig config_;
    bool fallback_mode_;
    
public:
    RobustTracker(const std::string& algorithm, const TrackConfig& config)
        : algorithm_name_(algorithm), config_(config), fallback_mode_(false)
    {
        try {
            tracker_ = TrackerFactory::createTracker(algorithm, config);
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize " << algorithm << ": " << e.what() << std::endl;
            
            // Fallback to SORT if other algorithms fail
            if (algorithm != "SORT") {
                std::cerr << "Falling back to SORT tracker" << std::endl;
                tracker_ = TrackerFactory::createTracker("SORT", config);
                fallback_mode_ = true;
            } else {
                throw;
            }
        }
    }
    
    std::vector<TrackedObject> update(const std::vector<Detection>& detections, 
                                     const cv::Mat& frame = cv::Mat()) {
        try {
            return tracker_->update(detections, frame);
        } catch (const std::exception& e) {
            std::cerr << "Tracking update failed: " << e.what() << std::endl;
            
            if (!fallback_mode_) {
                std::cerr << "Attempting to reinitialize tracker" << std::endl;
                
                try {
                    tracker_ = TrackerFactory::createTracker(algorithm_name_, config_);
                    return tracker_->update(detections, frame);
                } catch (const std::exception& reinit_error) {
                    std::cerr << "Reinitialization failed: " << reinit_error.what() << std::endl;
                    return {};  // Return empty result
                }
            }
            
            return {};  // Return empty result on fallback failure
        }
    }
    
    bool isFallbackMode() const { return fallback_mode_; }
    std::string getCurrentAlgorithm() const { 
        return fallback_mode_ ? "SORT" : algorithm_name_; 
    }
};
```

---

## 8. Performance Optimization 

### Profiling Integration

```cpp
class PerformanceProfiler {
private:
    struct ProfileData {
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::duration<double, std::milli> total_time{0};
        size_t call_count = 0;
    };
    
    std::unordered_map<std::string, ProfileData> profiles_;
    std::mutex mutex_;
    
public:
    class ScopedTimer {
    private:
        PerformanceProfiler* profiler_;
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;
        
    public:
        ScopedTimer(PerformanceProfiler* profiler, const std::string& name)
            : profiler_(profiler), name_(name), 
              start_(std::chrono::high_resolution_clock::now()) {}
        
        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start_);
            profiler_->recordTime(name_, duration);
        }
    };
    
    void recordTime(const std::string& name, std::chrono::duration<double, std::milli> duration) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& profile = profiles_[name];
        profile.total_time += duration;
        profile.call_count++;
    }
    
    void printReport() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "Performance Report:" << std::endl;
        std::cout << std::setw(20) << "Function" 
                  << std::setw(15) << "Total (ms)" 
                  << std::setw(10) << "Calls" 
                  << std::setw(15) << "Avg (ms)" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        for (const auto& [name, profile] : profiles_) {
            double avg_time = profile.call_count > 0 ? 
                profile.total_time.count() / profile.call_count : 0.0;
            
            std::cout << std::setw(20) << name 
                      << std::setw(15) << std::fixed << std::setprecision(3) << profile.total_time.count()
                      << std::setw(10) << profile.call_count
                      << std::setw(15) << std::fixed << std::setprecision(3) << avg_time << std::endl;
        }
    }
};

#define PROFILE_SCOPE(profiler, name) PerformanceProfiler::ScopedTimer timer(profiler, name)
```

### Optimized Tracker with Profiling

```cpp
class OptimizedTracker : public BaseTracker {
private:
    std::unique_ptr<BaseTracker> inner_tracker_;
    mutable PerformanceProfiler profiler_;
    
public:
    OptimizedTracker(std::unique_ptr<BaseTracker> tracker)
        : inner_tracker_(std::move(tracker)) {}
    
    std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, 
        const cv::Mat& frame = cv::Mat()
    ) override {
        PROFILE_SCOPE(&profiler_, "total_update");
        
        {
            PROFILE_SCOPE(&profiler_, "detection_filtering");
            // Any preprocessing could go here
        }
        
        std::vector<TrackedObject> results;
        {
            PROFILE_SCOPE(&profiler_, "core_tracking");
            results = inner_tracker_->update(detections, frame);
        }
        
        {
            PROFILE_SCOPE(&profiler_, "postprocessing");
            // Any postprocessing could go here
        }
        
        return results;
    }
    
    void printPerformanceReport() const {
        profiler_.printReport();
    }
    
    std::string getAlgorithmName() const override {
        return inner_tracker_->getAlgorithmName();
    }
};
```

> **Complete Implementation Details**: For full code implementations, detailed examples, and advanced patterns, see the [Code Examples](Code_Examples.md) documentation.
