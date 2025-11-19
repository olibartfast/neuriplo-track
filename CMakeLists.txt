cmake_minimum_required(VERSION 3.20)
project(multi_object_tracking)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include centralized version management
include(cmake/versions.cmake)

# Set DEFAULT_BACKEND as a CACHE variable
if(NOT DEFINED DEFAULT_BACKEND)
   set(DEFAULT_BACKEND "ONNX_RUNTIME" CACHE STRING "Default inference backend: OPENCV_DNN, ONNX_RUNTIME, LIBTORCH, TENSORRT, OPENVINO, LIBTENSORFLOW")
endif()

message(STATUS "Home path: $ENV{HOME}")

# Include dependency validation
include(cmake/DependencyValidation.cmake)

# Find system dependencies first
find_package(OpenCV REQUIRED)
find_package(glog REQUIRED)
find_package(Eigen3 REQUIRED)
message(STATUS "✓ OpenCV ${OpenCV_VERSION} found")
message(STATUS "✓ glog found")
message(STATUS "✓ Eigen3 found")

# Fetch external dependencies
include(FetchContent)

# Fetch object-detection-inference with specific version
set(USE_GSTREAMER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    object-detection-inference
    GIT_REPOSITORY https://github.com/olibartfast/object-detection-inference.git
    GIT_TAG        ${OBJECT_DETECTION_INFERENCE_VERSION}
)
FetchContent_MakeAvailable(object-detection-inference)
message(STATUS "object-detection-inference_SOURCE_DIR: ${object-detection-inference_SOURCE_DIR}")

# Fetch ByteTrack with specific version
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/trackers/ByteTrack)
set(BUILD_BYTETRACK_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ByteTrack
    GIT_REPOSITORY https://github.com/Vertical-Beach/ByteTrack-cpp.git
    GIT_TAG        ${BYTETRACK_VERSION}
)
FetchContent_GetProperties(ByteTrack)
if(NOT bytetrack_POPULATED)
    FetchContent_Populate(ByteTrack)
    add_subdirectory(${bytetrack_SOURCE_DIR} ${bytetrack_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
message(STATUS "bytetrack_SOURCE_DIR: ${bytetrack_SOURCE_DIR}")

# Validate all dependencies after they have been fetched
validate_all_dependencies()

# Define paths
set(TRACKERS_ROOT ${CMAKE_SOURCE_DIR}/trackers)

# Add module paths
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
message(STATUS "CMake module path: ${CMAKE_MODULE_PATH}")

# Add subdirectories for the trackers module
add_subdirectory(${TRACKERS_ROOT})

# Option to build only the trackers library
option(BUILD_ONLY_LIB "Build only the trackers library" OFF)

if(NOT BUILD_ONLY_LIB)
    # Add the app module subdirectory
    add_subdirectory(app)
endif()

# Print final configuration summary
message(STATUS "=== Build Configuration Summary ===")
message(STATUS "Backend: ${DEFAULT_BACKEND}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Only Library: ${BUILD_ONLY_LIB}")
message(STATUS "GStreamer Support: ${USE_GSTREAMER}")
message(STATUS "======================================")
