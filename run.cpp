#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include "YoloV5.hpp"
#include "KalmanTracker.hpp"
#include "Sort.hpp"

static const std::string params = "{ help h   |   | print help message }"
      "{ detector     |  yolov5x | mobilenet, svm, yolov4-tiny, yolov4, yolov5s, yolov5x | detector model}"
      "{ link l   |   | capture video from ip camera}"
      "{ labels lb   |  ../labels | path to class labels file}"
      "{ tracker tr   |  SORT | tracking algorithm}"
      "{ model_path mp   |  ../models | path to models}";


std::vector<TrackingBox> convertBbox(std::vector<t_prediction>& detection_results, 
    std::vector<std::string> classes, 
    std::vector<std::string> track_classes)
{
    // form detection box data for tracking
    std::vector<TrackingBox> detection_frame_data;
    for(auto detection_result : detection_results) {

        // check tracking class list if needed
        std::string detection_class = classes[detection_result.class_index];
        if(std::find(track_classes.begin(), track_classes.end(), detection_class) == track_classes.end()) {
            continue;
        }

        // change box to (xmin,ymin,xmax,ymax) format
        float x_min = detection_result.x;
        float y_min = detection_result.y;
        float x_max = detection_result.x + detection_result.width;
        float y_max = detection_result.y + detection_result.height;

        TrackingBox tb;
        tb.box = cv::Rect_<float>(cv::Point_<float>(x_min, y_min), cv::Point_<float>(x_max, y_max));
        detection_frame_data.push_back(tb);
    }

    return detection_frame_data;
}


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

auto modelSetup(const std::string& modelPath, const std::string& configName, const std::string& weigthName){
    const auto modelConfiguration = modelPath + "/" + configName;
    const auto modelBinary = modelPath + "/" + weigthName;
    if(!std::filesystem::exists(modelConfiguration) || !std::filesystem::exists(modelBinary))
    {
        std::cerr << "Wrong path to model " << std::endl;
        exit(1);
    }
    return std::make_tuple(modelConfiguration, modelBinary); 
}

std::unique_ptr<Detector> createDetector(
    const std::string& detectorType,
    const std::string& labelsPath,
    const std::string& modelPath){
    std::vector<std::string> classes; 
    std::string modelConfiguration; 
    std::string modelBinary;   
    if(detectorType.find("yolov5") != std::string::npos)  
    {
        std::string modelBinary;
        classes = readLabelNames(labelsPath);   
        return std::make_unique<YoloV5>(classes, "", modelPath);
    }
    return nullptr;
}     

std::unique_ptr<Tracker> createTracker(const std::string& trackingAlgorithm)
{
    if(trackingAlgorithm.find("SORT") != std::string::npos)  
    {   
        auto sortTracker = std::make_unique<Sort>();
        return sortTracker;
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

    std::vector<std::string> classes = readLabelNames(labelsPath);
    std::vector<std::string> track_classes{"person", "car"};

    // Open video file
    cv::VideoCapture cap(parser.get<std::string>("link"));
    std::unique_ptr<Detector> detector = createDetector(detectorType, labelsPath, modelPath); 
    std::unique_ptr<Tracker> tracker = createTracker(trackingAlgorithm);

    // Process video frames
    cv::Mat frame;
    while (cap.read(frame)) 
    {

        // Run multi-object tracker on frame
        std::vector<t_prediction> detections = detector->run_detection(frame);
        auto detection_frame_data = convertBbox(detections, classes, track_classes);
        tracker->track(detection_frame_data);
        auto tracksOutput =  tracker->getTrackingBoxes();

        // randomly generate colors, only for display
        cv::RNG rng(0xFFFFFFFF);
        cv::Scalar_<int> randColor[20];
        for (int i = 0; i < 20; i++)
            rng.fill(randColor[i], cv::RNG::UNIFORM, 0, 256);

        // show detection box in white
        for(auto detection_result : detections) 
        {
            // change box to (xmin,ymin,xmax,ymax) format
            float x_min = detection_result.x;
            float y_min = detection_result.y;
            float x_max = detection_result.x + detection_result.width;
            float y_max = detection_result.y + detection_result.height;

            cv::rectangle(frame, cv::Point(x_min, y_min), cv::Point(x_max, y_max), cv::Scalar(255,255,255), 2, 8, 0);
            cv::putText(frame, classes[detection_result.class_index], cv::Point(x_min + detection_result.width/2, y_min-15), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 255, 255), 2, 8, 0);
        }

        // show tracking box in color
        for (auto tb : tracksOutput) {
            cv::rectangle(frame, tb.box, randColor[tb.id % 20], 2, 8, 0);
            cv::putText(frame, std::to_string(tb.id), cv::Point(tb.box.x, tb.box.y-15), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 255, 255), 2, 8, 0);
        }
        
        cv::imshow("SORT", frame);
        cv::waitKey(1);

    }

    return 0;
}
