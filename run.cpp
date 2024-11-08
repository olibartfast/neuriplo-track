#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include "SortWrapper.hpp"
#include "ByteTrackWrapper.hpp"
#include "BoTSORTWrapper.hpp"

#include "DetectorSetup.hpp"
#include "InferenceBackendSetup.hpp"
#include "Detector.hpp"


static const std::string params = "{ help h   |   | print help message }"
      "{ detector     |  yolov5 | detector model}"
      "{ link l   |   | capture video from ip camera}"
      "{ labels lb   |  ../labels | path to class labels file}"
      "{ tracker tr   |  SORT | tracking algorithm}"
      "{ class cl   |  2 | class id from coco dataset to track}"
      "{ model_path mp   |  ../models | path to models}"
      "{ tracker_config tc   |  config/tracker.ini | path to tracker config file}"
      "{ gmc_config gc   |  config/gmc.ini | path to gmc config file}"
      "{ reid_config rc   |  config/reid.ini | path to reid config file}"
      "{ reid_onnx ro   |  models/reid.onnx | path to reid onnx file}";
  

std::vector<std::string> readLabelNames(const std::string& fileName)
{
    if(!std::filesystem::exists(fileName)){
        std::cerr << "Wrong path to labels " <<  fileName << std::endl;
        exit(1);
    } 
    std::vector<std::string> classes;
    std::ifstream ifs(fileName.c_str());
    std::string line;
    while (getline(ifs, line))
    classes.push_back(line);
    return classes;   
}
   
std::unique_ptr<BaseTracker> createTracker(const std::string& trackingAlgorithm, const TrackConfig& config)
{
    if(trackingAlgorithm == "BoTSORT")  
    {   
        return std::make_unique<BoTSORTWrapper>(config);
    }
    else if(trackingAlgorithm == "SORT")  
    {   
        return std::make_unique<SortWrapper>(config);
    }     
    else if(trackingAlgorithm == "ByteTrack")  
    {   
        return std::make_unique<ByteTrackWrapper>(config);
    }

    return nullptr;
}
int main(int argc, char** argv) {

    cv::CommandLineParser parser(argc, argv, params);
    parser.about("Multi object tracker");
    if (parser.has("help")){
      parser.printMessage();
      return 0;  
    }

    const std::string modelPath = parser.get<std::string>("model_path");
    const std::string labelsPath = parser.get<std::string>("labels");
    const std::string detectorType = parser.get<std::string>("detector");
    const std::string trackingAlgorithm = parser.get<std::string>("tracker");
    const int classToTrack = parser.get<int>("class");

    const std::string trackerConfigPath = parser.get<std::string>("tracker_config");
    const std::string gmcConfigPath = parser.get<std::string>("gmc_config");
    const std::string reidConfigPath = parser.get<std::string>("reid_config");
    const std::string reidOnnxPath = parser.get<std::string>("reid_onnx");   
        
    // Specify the class IDs to track, e.g., {1, 2} for classes "person" and "car"
    std::set<int> classes_to_track = {classToTrack};
    
    
    std::vector<std::string> classes = readLabelNames(labelsPath);


    // Open video file
    cv::VideoCapture cap(parser.get<std::string>("link"));
    const auto detector =  DetectorSetup::createDetector(detectorType);

    const auto engine = setup_inference_engine(modelPath, "", false);
    if (!engine) {
        throw std::runtime_error("Can't setup an inference engine for " + modelPath);
    }    

    TrackConfig config(classes_to_track, trackerConfigPath, gmcConfigPath, reidConfigPath, reidOnnxPath);
    std::unique_ptr<BaseTracker> tracker = createTracker(trackingAlgorithm, config);

    if(!detector || !tracker)
        std::exit(1);

    // randomly generate colors, only for display
    cv::RNG rng(0xFFFFFFFF);
    cv::Scalar_<int> randColor[20];
    for (int i = 0; i < 20; i++)
        rng.fill(randColor[i], cv::RNG::UNIFORM, 0, 256);

    // Process video frames
    cv::Mat frame;
    while (cap.read(frame)) 
    {

        const auto input_blob = detector->preprocess_image(frame);
        const auto [outputs, shapes] = engine->get_infer_results(input_blob);
        std::vector<Detection> detections = detector->postprocess(outputs, shapes, frame.size());
        auto tracksOutput = tracker->update(detections, frame);


        // show detection box in white
        for(auto detection_result : detections) 
        {
            cv::rectangle(frame, detection_result.bbox, cv::Scalar(255,255,255), 2, 8, 0);
            cv::putText(frame, classes[detection_result.label], cv::Point(detection_result.bbox.x + detection_result.bbox.width/2, detection_result.bbox.y-15), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 255, 255), 2, 8, 0);
        }

        // show tracking box in color
        for (auto tb : tracksOutput) {
            cv::rectangle(frame, cv::Rect(tb.x, tb.y, tb.width, tb.height), randColor[tb.track_id % 20], 4, 8, 0);
            cv::putText(frame, std::to_string(tb.track_id), cv::Point(tb.x, tb.y-15), cv::FONT_HERSHEY_PLAIN, 2, randColor[tb.track_id % 20], 4, 8, 0);
        }
        
        cv::imshow(trackingAlgorithm, frame);
        cv::waitKey(1);

    }

    return 0;
}
