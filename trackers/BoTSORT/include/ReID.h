#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <onnxruntime_cxx_api.h>
#include "DataType.h"

namespace botsort {
    
class ReIDModel {
public:
    ReIDModel(const std::string &config_path, const std::string &onnx_model_path);
    ~ReIDModel() = default;

    void pre_process(cv::Mat &image);
    FeatureVector extract_features(cv::Mat &image);

    const std::string &get_distance_metric() const {
        return _distance_metric;
    }

private:
    void _load_params_from_config(const std::string &config_path);
    void _initialize_onnx_session(const std::string &model_path);

private:
    cv::Size _input_size;
    std::string _onnx_model_path, _distance_metric;
    std::string _input_layer_name;
    bool _swap_rb;
    
    // ONNX Runtime members
    Ort::Env _env;
    Ort::SessionOptions _session_options;
    std::unique_ptr<Ort::Session> _session;
    std::vector<float> _input_tensor_values;
    std::vector<int64_t> _input_tensor_shape;
    std::vector<std::string> _input_node_names;
    std::vector<std::string> _output_node_names;
};
}