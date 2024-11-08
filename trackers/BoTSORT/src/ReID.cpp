#include "ReID.h"
#include "INIReader.h"
#include <iostream>

namespace botsort
{

ReIDModel::ReIDModel(const std::string &config_path, const std::string &onnx_model_path) 
    : _env(ORT_LOGGING_LEVEL_WARNING, "reid_model") {
    std::cout << "Initializing ReID model" << std::endl;
    _load_params_from_config(config_path);
    _onnx_model_path = onnx_model_path;
    _initialize_onnx_session(onnx_model_path);
}

void ReIDModel::_initialize_onnx_session(const std::string &model_path) {
    try {
        // Configure session options
        _session_options.SetIntraOpNumThreads(1);
        _session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Create session
        _session = std::make_unique<Ort::Session>(_env, model_path.c_str(), _session_options);

        // Set input and output names
        Ort::AllocatorWithDefaultOptions allocator;
        
        // Use smart pointers returned by GetInputNameAllocated and GetOutputNameAllocated
        Ort::AllocatedStringPtr input_name_ptr = _session->GetInputNameAllocated(0, allocator);
        Ort::AllocatedStringPtr output_name_ptr = _session->GetOutputNameAllocated(0, allocator);
        
        //_input_node_names.push_back(input_name_ptr.get());
         _input_node_names.emplace_back(_session->GetInputNameAllocated(0, allocator).get());
       // _output_node_names.push_back(output_name_ptr.get());
        _output_node_names.emplace_back(_session->GetOutputNameAllocated(0, allocator).get());

        // Get input shape
        auto input_shape = _session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        _input_tensor_shape = input_shape;
        
        // Allocate input tensor memory
        size_t input_tensor_size = 1;
        for (auto dim : input_shape) {
            if (dim > 0) input_tensor_size *= dim;
        }
        _input_tensor_values.resize(input_tensor_size);

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
        throw;
    }
}


FeatureVector ReIDModel::extract_features(cv::Mat &image_patch) {
    pre_process(image_patch);
    
    // Prepare input tensor
    cv::Mat blob;
    cv::dnn::blobFromImage(image_patch, blob, 1.0/255.0, _input_size, 
                          cv::Scalar(0,0,0), _swap_rb, false);
    
    std::memcpy(_input_tensor_values.data(), blob.ptr<float>(), 
                _input_tensor_values.size() * sizeof(float));

    // Create input tensor
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, _input_tensor_values.data(), _input_tensor_values.size(),
        _input_tensor_shape.data(), _input_tensor_shape.size());

    std::vector<const char*> input_names_char(_input_node_names.size());
    std::transform(_input_node_names.begin(), _input_node_names.end(), input_names_char.begin(),
        [](const std::string& str) { return str.c_str(); });

    std::vector<const char*> output_names_char(_output_node_names.size());
    std::transform(_output_node_names.begin(), _output_node_names.end(), output_names_char.begin(),
        [](const std::string& str) { return str.c_str(); });

    // Run inference
    auto output_tensors = _session->Run(
        Ort::RunOptions{nullptr}, 
        input_names_char.data(), 
        &input_tensor, 
        1, 
        output_names_char.data(), 
        1);

    // Extract features
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    FeatureVector feature_vector = FeatureVector::Zero(1, FEATURE_DIM);
    
    for (int i = 0; i < FEATURE_DIM; i++) {
        feature_vector(0, i) = output_data[i];
    }

    return feature_vector;
}

void ReIDModel::pre_process(cv::Mat &image) {
    cv::resize(image, image, _input_size);
}

void ReIDModel::_load_params_from_config(const std::string &config_path) {
    const std::string section_name = "ReID";
    INIReader reid_config(config_path);
    if (reid_config.ParseError() < 0) {
        std::cout << "Can't load " << config_path << std::endl;
        exit(1);
    }

    _distance_metric = reid_config.Get(section_name, "distance_metric", "euclidean");
    _input_layer_name = reid_config.Get(section_name, "input_layer_name", "input");
    _swap_rb = reid_config.GetBoolean(section_name, "swapRB", false);

    std::vector<int> input_dims = reid_config.GetList<int>(section_name, "input_layer_dimensions");
    _input_size = cv::Size(input_dims[3], input_dims[2]);
}

}