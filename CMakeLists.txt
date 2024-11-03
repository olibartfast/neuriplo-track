
cmake_minimum_required(VERSION 3.20)
project(multi_object_tracking LANGUAGES CXX)
SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch ByteTrack repository
include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/trackers/ByteTrack)

# Fetch ByteTrack repository into the trackers folder
FetchContent_Declare(
  ByteTrack
  GIT_REPOSITORY https://github.com/Vertical-Beach/ByteTrack-cpp.git
  GIT_TAG main  # Use the appropriate branch or tag
)

FetchContent_MakeAvailable(ByteTrack)
message(STATUS "ByteTrack: ${bytetrack_SOURCE_DIR}")


# Fetch Eigen library into the trackers folder
FetchContent_Declare(
  Eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0  # Replace with the appropriate version tag
)
FetchContent_MakeAvailable(Eigen)

# Confirm paths for ByteTrack and Eigen
message(STATUS "Eigen: ${eigen_SOURCE_DIR}")

# Set source files for YOLOv5 and SORT
set(YOLOV5_SRC detectors/YoloV5/YoloV5.cpp)
set(SORT_SRC trackers/SORT/Sort.cpp trackers/SORT/KalmanTracker.cpp trackers/SORT/Hungarian.cpp)

# Find OpenCV
find_package(OpenCV REQUIRED)

# Add the executable for the main application
add_executable(${PROJECT_NAME} run.cpp ${YOLOV5_SRC} ${SORT_SRC})

# Include directories for OpenCV, ByteTrack, Eigen, and project-specific headers
target_include_directories(${PROJECT_NAME} PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  include
  src
  detectors
  trackers/SORT
  ${bytetrack_SOURCE_DIR}/include
  ${eigen_SOURCE_DIR}
)

# Link libraries (OpenCV, standard C++ libraries, etc.)
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${OpenCV_LIBRARIES}
  bytetrack
  Eigen3::Eigen  # Link Eigen as an imported target
  -lstdc++
  -lpthread
  -ldl
)