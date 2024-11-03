
cmake_minimum_required(VERSION 3.5)
project(multi_object_tracking LANGUAGES CXX)
SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch ByteTrack repository
# include(FetchContent)
# FetchContent_Declare(
#   ByteTrack
#   GIT_REPOSITORY https://github.com/Vertical-Beach/ByteTrack-cpp.git
#   GIT_TAG main  # Use the appropriate branch or tag
# )
# FetchContent_MakeAvailable(ByteTrack)

set(YOLOV5_SRC  detectors/YoloV5/YoloV5.cpp)
set(SORT_SRC trackers/SORT/Sort.cpp trackers/SORT/KalmanTracker.cpp trackers/SORT/Hungarian.cpp)        

find_package(OpenCV)

add_executable(${PROJECT_NAME} run.cpp ${YOLOV5_SRC} ${SORT_SRC})
target_include_directories(${PROJECT_NAME}  PRIVATE ${OpenCV_INCLUDE_DIRS} include src detectors  trackers/SORT)
target_link_libraries(${PROJECT_NAME}   PRIVATE ${OpenCV_LIBRARIES} -lstdc++ -lpthread -ldl)