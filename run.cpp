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
      "{ detector_type     |  yolov5 | detector model}"
      "{ link l   |   | capture video from ip camera}"
      "{ labels lb   |  ../labels | path to class labels file}"
      "{ tracker tr   |  SORT | tracking algorithm}"
      "{ classes cl   |  car, person | classes label name from coco dataset to track}"
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

std::vector<std::string> splitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::set<int> mapClassesToIds(const std::vector<std::string>& classesToTrack, const std::vector<std::string>& allClasses) {
    std::set<int> classIds;
    for (const auto& classToTrack : classesToTrack) {
        auto it = std::find(allClasses.begin(), allClasses.end(), classToTrack);
        if (it != allClasses.end()) {
            classIds.insert(std::distance(allClasses.begin(), it));
        } else {
            std::cerr << "Warning: Class '" << classToTrack << "' not found in labels file." << std::endl;
        }
    }
    return classIds;
}

std::string generateOutputPath(const std::string& inputPath) {
    std::filesystem::path inputFilePath(inputPath);
    if (inputFilePath.extension().empty()) {
        return "output_processed.mp4";
    }
    return inputFilePath.stem().string() + "_processed" + inputFilePath.extension().string();
}

cv::VideoWriter setupVideoWriter(const cv::VideoCapture& cap, const std::string& outputPath) {
    cv::Size frame_size(
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT))
    );
    double fps = cap.get(cv::CAP_PROP_FPS);
    return cv::VideoWriter(outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frame_size);
}

void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections, const std::vector<std::string>& classes) {
    for (const auto& detection : detections) {
        cv::rectangle(frame, detection.bbox, cv::Scalar(255,255,255), 2, 8, 0);
        cv::putText(frame, classes[detection.label], 
                    cv::Point(detection.bbox.x + detection.bbox.width/2, detection.bbox.y-15), 
                    cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 255, 255), 2, 8, 0);
    }
}

void drawTracks(cv::Mat& frame, const std::vector<TrackedObject>& tracks, const std::vector<cv::Scalar_<int>>& colors) {
    for (const auto& track : tracks) {
        cv::rectangle(frame, cv::Rect(track.x, track.y, track.width, track.height), 
                      colors[track.track_id % colors.size()], 4, 8, 0);
        cv::putText(frame, std::to_string(track.track_id), 
                    cv::Point(track.x, track.y-15), cv::FONT_HERSHEY_PLAIN, 2, 
                    colors[track.track_id % colors.size()], 4, 8, 0);
    }
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
    const std::string classesToTrackString = parser.get<std::string>("classes");
    const std::string detectorType = parser.get<std::string>("detector_type");
    const std::string trackingAlgorithm = parser.get<std::string>("tracker");


    const std::string trackerConfigPath = parser.get<std::string>("tracker_config");
    const std::string gmcConfigPath = parser.get<std::string>("gmc_config");
    const std::string reidConfigPath = parser.get<std::string>("reid_config");
    const std::string reidOnnxPath = parser.get<std::string>("reid_onnx");   
    
    std::vector<std::string> classes = readLabelNames(labelsPath);
    std::vector<std::string> classesToTrack = splitString(classesToTrackString, ',');
    std::set<int> classes_to_track = mapClassesToIds(classesToTrack, classes);

    if (classes_to_track.empty()) {
        std::cerr << "Error: No valid classes to track." << std::endl;
        return 1;
    }

    // Open video file
    cv::VideoCapture cap(parser.get<std::string>("link"));
    const auto detector =  DetectorSetup::createDetector(detectorType);

    const auto engine = setup_inference_engine(modelPath, "", false);
    if (!engine) {
        throw std::runtime_error("Can't setup an inference engine for " + modelPath);
    }    

    TrackConfig config(classes_to_track, trackerConfigPath, gmcConfigPath, reidConfigPath, reidOnnxPath);
    std::unique_ptr<BaseTracker> tracker = createTracker(trackingAlgorithm, config);

       std::vector<cv::Scalar_<int>> randColors(20);
    cv::RNG rng(0xFFFFFFFF);
    for (auto& color : randColors)
        rng.fill(color, cv::RNG::UNIFORM, 0, 256);

    std::string outputPath = generateOutputPath(parser.get<std::string>("link"));
    cv::VideoWriter videoWriter = setupVideoWriter(cap, outputPath);

    cv::Mat frame;
    while (cap.read(frame)) {
        const auto input_blob = detector->preprocess_image(frame);
        const auto [outputs, shapes] = engine->get_infer_results(input_blob);
        std::vector<Detection> detections = detector->postprocess(outputs, shapes, frame.size());
        auto tracksOutput = tracker->update(detections, frame);

        drawDetections(frame, detections, classes);
        drawTracks(frame, tracksOutput, randColors);
        
        videoWriter.write(frame);
        cv::imshow(trackingAlgorithm, frame);
        if (cv::waitKey(1) == 27) // Exit if ESC key is pressed
            break;
    }

    videoWriter.release();
    return 0;
}

