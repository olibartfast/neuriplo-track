
cmake_minimum_required(VERSION 3.5)
project(multi_object_tracking LANGUAGES CXX)
SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YOLOV5_SRC  Detectors/YoloV5/YoloV5.cpp)
set(SORT_SRC Trackers/SORT/Sort.cpp Trackers/SORT/KalmanTracker.cpp Trackers/SORT/Hungarian.cpp)        

find_package(OpenCV)

add_executable(${PROJECT_NAME} run.cpp ${YOLOV5_SRC} ${SORT_SRC})
target_include_directories(${PROJECT_NAME}  PRIVATE ${OpenCV_INCLUDE_DIRS} Detectors Trackers)
target_link_libraries(${PROJECT_NAME}   PRIVATE ${OpenCV_LIBRARIES} -lstdc++ -lpthread -ldl)