//
// ScriptedPropagator.hpp: a MaskPropagator test double driven by a per-frame
// script, so the SAM3 tracker loop is testable with no weights and no video.
//
// Each propagate() call consumes the next scripted frame. addObject() hands out
// ids sequentially from 0 in call order, so a script names the ids the tracker
// is expected to have birthed by that frame. Every mutating call is recorded
// for assertion.
//
#pragma once
#include "MaskPropagator.hpp"

#include <cstddef>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <utility>
#include <vector>

struct ScriptedObject {
    int id{};
    cv::Rect_<float> box{};
    float presence{1.0f};
    // Foreground within `box`; defaults to the whole box. A narrower band lets
    // a test separate the mask cue from the box cue the way
    // testMaskCueDecides does for the motion trackers.
    cv::Rect_<float> foreground{};
    bool has_foreground{};

    ScriptedObject(int id_, const cv::Rect_<float> &box_, float presence_ = 1.0f)
        : id(id_), box(box_), presence(presence_), foreground(box_), has_foreground(false) {}
    ScriptedObject(int id_, const cv::Rect_<float> &box_, const cv::Rect_<float> &foreground_, float presence_ = 1.0f)
        : id(id_), box(box_), presence(presence_), foreground(foreground_), has_foreground(true) {}
};

class ScriptedPropagator : public tracking::MaskPropagator {
  public:
    explicit ScriptedPropagator(int capacity = 64) : capacity_(capacity) {}

    void setScript(const std::vector<std::vector<ScriptedObject>> &frames) {
        script_ = frames;
        frame_ = 0;
    }

    std::vector<tracking::PropagatedMask> propagate(const cv::Mat &) override {
        ++propagate_calls_;
        std::vector<tracking::PropagatedMask> out;
        if (frame_ < script_.size()) {
            for (const auto &object : script_[frame_]) {
                const cv::Rect_<float> foreground = object.has_foreground ? object.foreground : object.box;
                tracking::PropagatedMask propagated;
                propagated.id = object.id;
                propagated.presence = object.presence;
                const int width = static_cast<int>(foreground.width);
                const int height = static_cast<int>(foreground.height);
                if (width > 0 && height > 0) {
                    propagated.mask.mask = cv::Mat::ones(height, width, CV_8U) * 255;
                    propagated.mask.origin = cv::Point(static_cast<int>(foreground.x), static_cast<int>(foreground.y));
                    propagated.mask.foreground = width * height;
                }
                out.push_back(std::move(propagated));
            }
        }
        ++frame_;
        return out;
    }

    int addObject(const cv::Rect_<float> &box, const cv::Mat &) override {
        added_boxes_.push_back(box);
        return next_id_++;
    }

    void reprompt(int id, const cv::Rect_<float> &box, const cv::Mat &) override { reprompted_.emplace_back(id, box); }

    void removeObject(int id) override { removed_ids_.push_back(id); }

    int capacity() const override { return capacity_; }

    const std::vector<cv::Rect_<float>> &addedBoxes() const { return added_boxes_; }
    const std::vector<std::pair<int, cv::Rect_<float>>> &reprompted() const { return reprompted_; }
    const std::vector<int> &removedIds() const { return removed_ids_; }
    std::size_t propagateCalls() const { return propagate_calls_; }

  private:
    int capacity_;
    std::vector<std::vector<ScriptedObject>> script_;
    std::size_t frame_{};
    std::size_t propagate_calls_{};
    int next_id_{};
    std::vector<cv::Rect_<float>> added_boxes_;
    std::vector<std::pair<int, cv::Rect_<float>>> reprompted_;
    std::vector<int> removed_ids_;
};
