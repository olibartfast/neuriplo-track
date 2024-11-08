cmake_minimum_required(VERSION 3.20)
project(multi_object_tracking LANGUAGES CXX)

SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use system-installed Eigen
find_package(Eigen3 REQUIRED)

# Set ONNX Runtime paths based on local installation
set(ONNXRUNTIME_DIR "$ENV{HOME}/onnxruntime-linux-x64-gpu-1.19.2")
set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_DIR}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")

# Verify ONNX Runtime installation
if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIRS}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found in ${ONNXRUNTIME_INCLUDE_DIRS}")
endif()

# Find main ONNX Runtime library
find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS ${ONNXRUNTIME_LIB_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

# Find ONNX Runtime GPU provider
find_library(ONNXRUNTIME_GPU_LIBRARY
    NAMES onnxruntime_providers_cuda
    PATHS ${ONNXRUNTIME_LIB_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "Found ONNX Runtime GPU: ${ONNXRUNTIME_GPU_LIBRARY}")

# Fetch ByteTrack repository
include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/trackers/ByteTrack)
FetchContent_Declare(
    ByteTrack
    GIT_REPOSITORY https://github.com/Vertical-Beach/ByteTrack-cpp.git
    GIT_TAG main  # Use the appropriate branch or tag
)
FetchContent_MakeAvailable(ByteTrack)
message(STATUS "ByteTrack: ${bytetrack_SOURCE_DIR}")

# Confirm paths for Eigen
message(STATUS "Using system-installed Eigen3: ${EIGEN3_INCLUDE_DIR}")

# Set source files for YOLOv5 and SORT
set(YOLOV5_SRC detectors/YoloV5/YoloV5.cpp)
set(SORT_SRC 
    trackers/SORT/Sort.cpp 
    trackers/SORT/KalmanTracker.cpp 
    trackers/SORT/Hungarian.cpp
)
file(GLOB_RECURSE BOTSORT_SRC "trackers/BoTSORT/src/*.cpp")

# Find OpenCV
find_package(OpenCV REQUIRED)

# Add the executable for the main application
add_executable(${PROJECT_NAME} 
    run.cpp 
    ${YOLOV5_SRC} 
    ${SORT_SRC} 
    ${BOTSORT_SRC}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
    include
    src
    detectors
    trackers/SORT
    trackers/BoTSORT/include
    ${bytetrack_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBRARIES}
    ${ONNXRUNTIME_LIBRARY}
    ${ONNXRUNTIME_GPU_LIBRARY}
    bytetrack
    Eigen3::Eigen
    -lstdc++
    -lpthread
    -ldl
)

# Add compile definition for GPU support
target_compile_definitions(${PROJECT_NAME} PRIVATE
    HAVE_ONNXRUNTIME_GPU
)