# Tracking Algorithms Implementation Guide

This document provides detailed information about the tracking algorithms implemented in the Multi-Object Tracking system.

## Table of Contents
1. [SORT (Simple Online and Realtime Tracking)](#sort)
2. [BoTSORT (Bootstrapping and Track re-IDentification)](#botsort)
3. [ByteTrack](#bytetrack)
4. [OC-SORT (Observation-Centric SORT)](#ocsort)
5. [C-BIoU (Cascaded Buffered IoU)](#cbiou)
6. [Algorithm Comparison](#comparison)

## Related Documentation
- **[Code Examples](Code_Examples.md)** - Detailed implementation code, configuration files, and integration patterns
- **[System Architecture](System_Architecture.md)** - C++ design patterns and system structure

---

## 1. SORT (Simple Online and Realtime Tracking) {#sort}

**Characteristics**:
- Minimal and fast approach
- Based on Kalman Filter for prediction
- Association via Hungarian Algorithm with IoU
- Ideal for real-time applications

**C++ Implementation**:
```cpp
class Sort {
private:
    unsigned int max_age;
    int min_hits;
    double iouThreshold;
    std::vector<KalmanTracker> trackers;
    
public:
    std::vector<TrackingBox> update(const std::vector<TrackingBox>& detections);
};
```

### Algorithm Overview

SORT follows a straightforward tracking-by-detection approach:

1. **Prediction Phase**: Predicts track positions using constant velocity Kalman Filter
2. **Association Phase**: Associates detections to existing tracks using IoU and Hungarian algorithm
3. **Update Phase**: Updates track states with matched detections
4. **Management Phase**: Creates new tracks for unmatched detections, removes old tracks

### Key Components

- **Kalman Filter**: Uses 7-dimensional state vector (position, scale, aspect ratio, velocities)
- **Hungarian Algorithm**: Solves optimal assignment problem for detection-track matching
- **IoU Matching**: Primary similarity metric for association decisions
- **Track Lifecycle**: Simple age-based track management with hit/miss counters

### Technical Approach

- **State Representation**: Center coordinates, scale, aspect ratio, and their velocities
- **Motion Model**: Assumes constant velocity with Gaussian noise
- **Association Strategy**: Maximum IoU matching with configurable threshold
- **Track Initialization**: Immediate track creation for unassociated detections

### Advantages
- **High speed**: 100+ FPS on modern hardware
- **Implementation simplicity**: Minimal code complexity
- **Low computational requirements**: Suitable for resource-constrained environments
- **Proven reliability**: Widely tested baseline

### Limitations
- **No Re-ID handling**: No appearance features for robust association
- **Problematic with long occlusions**: Cannot recover tracks after extended disappearance
- **Frequent ID switches**: In complex scenes with similar objects
- **Pure motion model**: Relies only on motion prediction, vulnerable to erratic movements

---

## 2. BoTSORT (Bootstrapping and Track re-IDentification) {#botsort}

**Characteristics**:
- Extension of ByteTrack with Re-identification
- Appearance features for robust association
- Global Motion Compensation (GMC)
- Advanced occlusion handling

### Algorithm Overview

BoTSORT enhances ByteTrack with sophisticated re-identification and motion compensation:

### Key Innovations

#### 1. Re-identification Integration
- **Deep Features**: CNN-based appearance features for robust object association
- **Feature Smoothing**: Exponential moving average of appearance features over time
- **Multi-modal Matching**: Combines motion prediction with appearance similarity
- **Feature History**: Maintains appearance history for recovery after long occlusions

#### 2. Global Motion Compensation (GMC)
- **Camera Motion Estimation**: Detects and compensates for global camera movement
- **Feature-based Tracking**: Uses ORB/SIFT features for motion estimation
- **Homography Correction**: Applies geometric transformation to track predictions
- **Robust Estimation**: RANSAC-based outlier rejection for stable motion estimates

#### 3. Advanced Track Management
- **Multi-state Lifecycle**: New, Tracked, Lost, LongLost, Removed states
- **Adaptive Thresholding**: Different confidence thresholds for different track states
- **Class ID Tracking**: Maintains object classification with weighted voting
- **Track Recovery**: Re-identifies lost tracks using appearance features

#### 4. Hierarchical Association
- **Primary Matching**: High-confidence detections with tracked tracks (IoU + appearance)
- **Secondary Matching**: Low-confidence detections with lost tracks (appearance-based)
- **Tertiary Recovery**: Long-term re-identification for extended occlusions

### Tracking Pipeline

1. **Feature Extraction**: Extract Re-ID features for each detection
2. **Motion Prediction**: Kalman Filter prediction with GMC compensation
3. **Multi-stage Association**:
   - High-confidence detections with tracked tracks (IoU + appearance)
   - Low-confidence detections with lost tracks (appearance-based)
   - Remaining detections create new tracks
4. **Track Management**: Update states, handle track lifecycle

### Re-identification Module

**Feature Extraction**:
```cpp
FeatureVector BoTSORT::_extract_features(const cv::Mat& frame,
                                        const cv::Rect_<float>& bbox_tlwh) {
    // Crop detection region
    cv::Rect roi = bbox_to_rect(bbox_tlwh);
    cv::Mat crop = frame(roi);
    
    // Preprocess for ReID model
    cv::Mat input = preprocess_reid(crop);
    
    // ONNX inference
    auto features = reid_model->inference(input);
    return features;
}
```

**Appearance Distance Calculation**:
```cpp
float appearance_distance(const FeatureVector& feat1, const FeatureVector& feat2) {
    // Cosine similarity
    float dot_product = feat1.dot(feat2);
    float norm1 = feat1.norm();
    float norm2 = feat2.norm();
    return 1.0f - (dot_product / (norm1 * norm2));
}
```

### Advanced Features

#### Track State Management
```cpp
void Track::mark_lost() { state = TrackState::Lost; }
void Track::mark_long_lost() { state = TrackState::LongLost; }
void Track::mark_removed() { state = TrackState::Removed; }

// Automatic state transitions based on track age
if (time_since_update > max_time_lost) {
    mark_long_lost();
}
```

#### Class ID Management
```cpp
void Track::_update_class_id(uint8_t class_id, float score) {
    _class_hist.push_back({class_id, score});
    
    // Keep history size limited
    if (_class_hist.size() > max_class_history) {
        _class_hist.erase(_class_hist.begin());
    }
    
    // Update class based on weighted frequency
    std::map<uint8_t, float> class_weights;
    for (const auto& entry : _class_hist) {
        class_weights[entry.first] += entry.second;
    }
    
    // Select class with highest weighted frequency
    auto best_class = std::max_element(class_weights.begin(), class_weights.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    _class_id = best_class->first;
}
```

### Advantages
- **Robust re-identification**: Deep features enable tracking through occlusions
- **Advanced occlusion handling**: Multi-stage recovery mechanisms
- **Global camera motion compensation**: Handles camera movement effectively
- **High performance on complex datasets**: Excellent results on MOT17/20
- **Flexible association**: Combines motion and appearance cues

### Limitations
- **Higher computational requirements**: ReID model adds inference overhead
- **Dependency on pre-trained Re-ID model**: Requires domain-appropriate features
- **Implementation complexity**: More sophisticated codebase
- **Memory usage**: Feature history storage increases memory footprint

---

## 3. ByteTrack {#bytetrack}

**Characteristics**:
- Use of low-confidence detections
- Hierarchical association (high-conf → low-conf)
- Robustness to imperfect detections
- Balance between accuracy and speed

### Core Innovation

ByteTrack's key insight is leveraging low-confidence detections typically discarded by other trackers. This simple but effective approach significantly improves tracking continuity.

### Algorithm Strategy

#### 1. Confidence-based Detection Processing
- **High-confidence Detections**: Primary associations with existing tracks
- **Low-confidence Detections**: Secondary recovery mechanism for lost tracks
- **Hierarchical Matching**: Two-stage association process prevents incorrect matches
- **Adaptive Thresholding**: Different confidence levels for different association stages

#### 2. Two-stage Association
- **Stage 1**: High-confidence detections matched to active tracks using IoU
- **Stage 2**: Low-confidence detections matched to unmatched tracks with relaxed thresholds
- **Kalman Prediction**: Enhanced motion prediction for better association
- **Track Recovery**: Recovers temporarily lost tracks using weak detections

#### 3. Enhanced Track Management
- **Improved Lifecycle**: Better track initialization and termination logic
- **Buffer Management**: Configurable track buffer for handling temporary losses
- **State Transitions**: Smooth transitions between tracked/lost states
- **Noise Resilience**: Robust handling of detection noise and false positives

### Key Features

#### Confidence-based Processing
- **High confidence threshold**: 0.6-0.7 (typical)
- **Low confidence threshold**: 0.1-0.3 (typical)
- **Dynamic thresholding**: Adaptive based on scene complexity

#### Kalman Filter Enhancement
- Similar to SORT but with better track management
- Improved track initialization and deletion logic
- Enhanced state prediction for lost tracks

### Advantages
- **Recovers "lost" tracks**: Low-confidence detections help maintain continuity
- **Robust to noise**: Hierarchical matching reduces false associations
- **Competitive performance**: Good results on MOT17/20 benchmarks
- **Moderate complexity**: More sophisticated than SORT, simpler than BoTSORT
- **Speed efficiency**: Faster than appearance-based methods

### Limitations
- **Still no appearance features**: Relies purely on motion and IoU
- **Parameter sensitivity**: Confidence thresholds need tuning per dataset
- **Limited long-term tracking**: Cannot handle extended occlusions well

---

## 4. OC-SORT (Observation-Centric SORT) {#ocsort}

**Characteristics**:
- Motion-only: no appearance features, no Re-ID model, no extra weights
- Built directly on the SORT state model, so it costs little over SORT
- Designed for objects that disappear and come back
- Implemented in-tree from the paper ([arXiv 2203.14360](https://arxiv.org/abs/2203.14360))

### The problem it addresses

SORT's weakness is not the association metric but what happens while a track is
unmatched. The Kalman filter keeps integrating its own predictions, so the
estimate drifts, and the drift is *itself* used as the reference for the next
association. A short occlusion is therefore enough to lose an identity
permanently, and the longer the gap the worse the estimate that has to bridge it.

OC-SORT's answer is to treat observations, not filter state, as the anchor.

### Key Innovations

#### 1. ORU — Observation-centric Re-Update
When a lost track is re-associated, the filter is not simply corrected with the
new box. A virtual trajectory is interpolated between the last real observation
and the new one, and the filter is replayed over it. The accumulated error from
the prediction-only frames never enters the state.

#### 2. OCM — Observation-centric Momentum
Association cost combines IoU with the consistency between the track's direction
of travel and the direction from its last observation to the candidate detection.
The direction is measured across `delta_t` frames rather than consecutive ones,
which makes it far less sensitive to detection noise. The term is weighted by
`inertia` and by the detection score, so it biases the assignment without ever
overruling IoU on its own.

#### 3. OCR — Observation-centric Recovery
A second association round matches detections still unclaimed against the *last
observations* of tracks that stayed unmatched. This is what recovers an object
that reappears close to where it was last actually seen.

### Tracking Pipeline

1. **Gate detections** by `det_thresh`; low-scoring boxes are dropped, not used
   to keep a track alive
2. **Predict** every track and discard states that have become unusable
3. **Associate** with the OCM cost (Hungarian assignment), rejecting pairs below
   `iou_threshold`
4. **Recover** leftovers against last observations (OCR)
5. **Update** matched tracks (with ORU when they were lost), age the rest
6. **Create** tracks for unclaimed detections, delete tracks older than `max_age`

Reported boxes are the last real observation when there is one, matching the
paper's observation-centric output rule.

### Advantages
- **Recovers identities across occlusions** without any appearance model
- **Cheap**: one extra Hungarian round and a direction term over SORT
- **No extra assets**: no Re-ID ONNX file, no configuration files

### Limitations
- **Motion-only**: two similar objects that swap places while both are occluded
  cannot be told apart
- **`det_thresh` gates a second time**: with a low `--min_confidence` and the
  default 0.6, weak detections are silently discarded
- **Linear motion assumption** still underlies the Kalman prediction

---

## 5. C-BIoU (Cascaded Buffered IoU) {#cbiou}

**Characteristics**:
- No Kalman filter at all
- Motion-only, appearance-free
- Aimed at fast, irregular, non-linear motion
- Implemented in-tree from the paper ([arXiv 2211.14317](https://arxiv.org/abs/2211.14317))

### The problem it addresses

A constant-velocity Kalman filter does not merely fail on erratic motion — it
fails *confidently*, predicting a precise box in the wrong place. For sports
footage or any target that changes direction abruptly, a coarse estimate with a
generous matching radius beats a sharp estimate pointed the wrong way.

### Key Innovations

#### 1. Buffered IoU
Both the track box and the detection box are expanded by a scale factor before
IoU is computed. The buffer *is* the search radius: boxes that no longer overlap
at all can still be associated, and how much slack is allowed is one number.

#### 2. Cascaded matching
Association runs twice. The first round uses the tight buffer `b1`, so confident
pairs are settled before a wider radius can steal them; the second round retries
whatever is left at the larger buffer `b2`. This ordering is what keeps the
larger buffer from creating false matches in crowded scenes.

#### 3. Mean-displacement motion model
The predicted state is the last observation plus the average per-frame
displacement over the previous `motion_n` observations, extended one step per
missed frame. No filter, no covariance, no tuning of process noise.

### Tracking Pipeline

1. **Predict** each track from its observation history
2. **Match** at buffer `b1` (Hungarian over `1 - buffered IoU`)
3. **Match the remainder** at buffer `b2`
4. **Update** matched tracks, age the rest
5. **Create** tracks for unclaimed detections, delete tracks older than `max_age`

### Advantages
- **Handles irregular motion** that breaks constant-velocity predictions
- **Simplest of the five**: no filter, no appearance model, no configuration files
- **Two intuitive knobs**: how far to search, and how much history to average

### Limitations
- **Large buffers invite false matches** in dense scenes; `b2` is the risk knob
- **No appearance cue**, so identical objects crossing can still swap
- **History-based motion** needs a few observations before the model is useful

---

## 6. Algorithm Comparison {#comparison}

### Performance Comparison

| Metric | SORT | ByteTrack | OC-SORT | C-BIoU | BoTSORT |
|--------|------|-----------|---------|--------|---------|
| **Speed (FPS)** | 100+ | 30-60 | 80+ | 100+ | 10-30 |
| **Memory Usage** | Low | Medium | Low | Low | High |
| **Implementation Complexity** | Low | Medium | Medium | Low | High |
| **Extra assets required** | None | None | None | None | Re-ID ONNX + INI files |

For accuracy, see the externally measured HOTA figures below rather than the
rough MOTA/IDF1 ranges this table used to carry.

### External Benchmark Reference

The numbers above are indicative. For an independently measured, apples-to-apples
comparison across algorithms, use the [Roboflow `trackers` benchmarks](https://trackers.roboflow.com/)
([repo](https://github.com/roboflow/trackers)), which reports HOTA on MOT17,
SportsMOT, SoccerNet and DanceTrack with default parameters:

| Algorithm | MOT17 HOTA | SportsMOT HOTA | SoccerNet HOTA | DanceTrack HOTA | Available here |
|-----------|-----------:|---------------:|---------------:|----------------:|----------------|
| [SORT](https://arxiv.org/abs/1602.00763) | 58.4 | 70.8 | 81.6 | 47.2 | ✅ |
| [ByteTrack](https://arxiv.org/abs/2110.06864) | 60.1 | 73.0 | 84.0 | 53.3 | ✅ |
| [OC-SORT](https://arxiv.org/abs/2203.14360) | 61.9 | 71.7 | 78.4 | 54.1 | ✅ |
| [BoT-SORT](https://arxiv.org/abs/2206.14651) | 63.7 | 73.8 | 84.5 | 57.8 | ✅ |
| [C-BIoU](https://arxiv.org/abs/2211.14317) | 63.0 | 73.1 | 82.6 | 56.7 | ✅ |
| [McByte](https://arxiv.org/abs/2506.01373) | **64.1** | **76.5** | **85.0** | **67.2** | ❌ |

Read these as relative ordering, not as targets for this project's C++
implementations:

- **Detections differ per dataset.** MOT17, SportsMOT and DanceTrack use YOLOX
  detections; SoccerNet uses oracle ground-truth boxes, which is why its scores
  sit roughly 17-24 HOTA above MOT17. Tracking metrics move with detector quality.
- **Default parameters only.** The comparison page also reports grid-searched
  configurations, where the gaps between trackers narrow considerably (on MOT17,
  tuned SORT reaches 60.4 against ByteTrack's 60.5).
- **DanceTrack is a different split.** Its numbers are validation-set results
  (test-set evaluation is unavailable since the CodaLab shutdown), while the
  other three columns are test-set.
- **The McByte row has a different basis.** It is author-reported
  ([roboflow/trackers#513](https://github.com/roboflow/trackers/pull/513))
  against a BoT-SORT-without-Re-ID baseline rather than measured in the same
  five-tracker sweep, though that baseline matches the BoT-SORT run above.
- **Source:** the `trackers` comparison page as read on 2026-08-18, which
  corresponds to upstream release 2.6.0. (The page banner still reads v2.3.0,
  but the BoT-SORT and C-BIoU rows only exist from 2.6.0 onward.)

The ✅ column says an algorithm is available here, **not** that this C++ port
reproduces the score next to it. These numbers come from the Roboflow Python
implementations with their own detections; nothing in this repository has been
evaluated on MOT17. Producing comparable numbers is
[Phase 3](../specs/roadmap.md) work.

### Feature Comparison

| Feature | SORT | ByteTrack | OC-SORT | C-BIoU | BoTSORT |
|---------|------|-----------|---------|--------|---------|
| **Motion Model** | Kalman Filter | Enhanced Kalman | Kalman + observation re-update | Mean displacement (no filter) | Kalman + GMC |
| **Appearance Model** | None | None | None | None | Deep ReID |
| **Association Method** | IoU only | IoU hierarchical | IoU + direction (OCM), then last-observation IoU | Buffered IoU, cascaded | IoU + Appearance |
| **Occlusion Handling** | Basic | Improved | Advanced (ORU/OCR) | Improved (buffered match) | Advanced |
| **ID Switch Robustness** | Poor | Good | Good | Good | Excellent |
| **Irregular Motion** | Poor | Poor | Fair | Good | Fair |
| **Real-time Performance** | Excellent | Good | Excellent | Excellent | Fair |

> **Note**: For detailed implementation code, configuration examples, and integration patterns, see the [Code Examples](Code_Examples.md) documentation.

### Use Case Recommendations

#### SORT - Best for:
- **Real-time applications** with strict latency requirements
- **Simple scenarios** with minimal occlusions
- **Resource-constrained environments** (embedded systems, mobile)
- **Proof-of-concept** and baseline implementations
- **High frame rate** processing (>60 FPS required)

#### ByteTrack - Best for:
- **General-purpose tracking** with moderate complexity
- **Scenarios with detection noise** or inconsistent detector performance
- **Applications needing balance** between speed and accuracy
- **When appearance features** are not available or applicable
- **Medium-scale deployments** with standard hardware

#### OC-SORT - Best for:
- **Objects that get occluded** and reappear (crowds, foreground obstacles)
- **Wanting BoTSORT-class recovery** without shipping a Re-ID model
- **Pedestrian and vehicle scenes** where motion is mostly smooth
- **Upgrading from SORT** with minimal added cost

#### C-BIoU - Best for:
- **Fast or irregular motion** that breaks constant-velocity prediction
- **Sports and animal footage** where targets change direction abruptly
- **Low-frame-rate input**, where objects move far between frames
- **The simplest possible tracker** that still handles hard motion

#### BoTSORT - Best for:
- **High-accuracy requirements** where ID consistency is critical
- **Complex scenes** with frequent occlusions
- **Long-term tracking** scenarios
- **Surveillance applications** where re-identification is important
- **Research and development** of advanced tracking systems

### Configuration Guidelines

#### SORT Configuration
- **IoU Threshold** (0.3): Minimum overlap for detection-track association
- **Max Age** (30): Maximum frames to keep lost tracks before deletion
- **Min Hits** (3): Required consecutive detections before track confirmation

#### ByteTrack Configuration  
- **High Confidence** (0.6): Primary detection threshold for first-stage matching
- **Low Confidence** (0.1): Secondary threshold for recovery matching
- **Match Threshold** (0.8): IoU threshold for track-detection association
- **Track Buffer** (30): Frames to maintain lost tracks for potential recovery

#### OC-SORT Configuration
- **Max Age** (30): Frames a track survives unmatched; raise it for long occlusions
- **Delta T** (3): Frame gap used to estimate direction; raise it when detections are jittery
- **Inertia** (0.2): Weight of the direction term; raise it when objects move consistently
- **Det Threshold** (0.6): Second gate on detection score, applied after `--min_confidence`

#### C-BIoU Configuration
- **Buffer b1** (0.3): First-round search radius as a fraction of box size
- **Buffer b2** (0.5): Second-round radius; the main risk knob in crowded scenes
- **Motion N** (5): Observations averaged by the motion model; lower reacts faster, higher is steadier
- **Max Age** (30): Frames a track survives unmatched

#### BoTSORT Configuration
- **Detection Thresholds**: Similar to ByteTrack for hierarchical processing
- **ReID Model Path**: ONNX model for appearance feature extraction
- **GMC Method**: Feature detector (ORB, SIFT, ECC) for motion compensation
- **Feature Parameters**: Batch size, feature dimensions, and matching thresholds

> **Detailed Configuration**: See [Code Examples](Code_Examples.md) for complete configuration files and loading mechanisms.

### Integration Patterns

#### Simple Integration (SORT)
- **Minimal Setup**: Basic configuration with IoU thresholds
- **Fast Processing**: Suitable for real-time applications
- **Limited Dependencies**: Only requires detection input
- **Lightweight**: Low memory and computational overhead

#### Moderate Integration (ByteTrack)
- **Balanced Approach**: Good performance with moderate complexity
- **Hierarchical Processing**: Two-stage detection confidence handling
- **Enhanced Recovery**: Better track continuity than SORT
- **Configurable Thresholds**: Tunable for different scenarios

#### Motion-Only Recovery (OC-SORT, C-BIoU)
- **No extra assets**: no Re-ID model, no INI files; `--tracker=OCSORT` or `--tracker=CBIoU` is enough
- **Tunable from the CLI**: every parameter is exposed as a flag
- **Frame not required**: like SORT and ByteTrack, they ignore the image
- **Longer default lifetime**: `max_age` defaults to 30 so a track can survive a gap

#### Advanced Integration (BoTSORT)
- **Full-featured Setup**: Requires ReID model and configuration files
- **Frame Processing**: Needs input frames for appearance feature extraction
- **Model Dependencies**: ReID ONNX model and GMC configuration
- **Maximum Accuracy**: Best performance for complex scenarios

> **Complete Integration Examples**: See [Code Examples](Code_Examples.md) for detailed implementation patterns, multi-camera setups, and optimization techniques.

This guide provides the conceptual foundation for understanding and choosing between the five tracking algorithms. Each serves different use cases, from high-speed real-time applications through occlusion- and irregular-motion-heavy scenes to accuracy-critical scenarios requiring robust re-identification capabilities.