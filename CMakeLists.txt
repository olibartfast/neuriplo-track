cmake_minimum_required(VERSION 3.20)
project(multi_object_tracking LANGUAGES CXX)

# Define options
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)

# Print option values for verification
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(glog REQUIRED)
find_package(Eigen3 REQUIRED)

include(FetchContent)

# ONNX Runtime setup - Use environment variable or download
set(ONNXRUNTIME_VERSION "1.19.2")

# Check if ONNX_RUNTIME_DIR is set via environment
if(DEFINED ENV{ONNX_RUNTIME_DIR})
    set(ONNXRUNTIME_DIR "$ENV{ONNX_RUNTIME_DIR}")
    message(STATUS "Using ONNX Runtime from environment: ${ONNXRUNTIME_DIR}")
else()
    # Default path following neuriplo convention
    if(WIN32)
        set(DEFAULT_DEPENDENCY_ROOT "$ENV{USERPROFILE}/dependencies")
    else()
        set(DEFAULT_DEPENDENCY_ROOT "$ENV{HOME}/dependencies")
    endif()
    
    set(ONNXRUNTIME_DIR "${DEFAULT_DEPENDENCY_ROOT}/onnxruntime-linux-x64-gpu-${ONNXRUNTIME_VERSION}")
    
    # Download if not exists
    if(NOT EXISTS "${ONNXRUNTIME_DIR}")
        message(STATUS "Downloading ONNX Runtime ${ONNXRUNTIME_VERSION}...")
        file(DOWNLOAD
            "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-gpu-${ONNXRUNTIME_VERSION}.tgz"
            "${CMAKE_BINARY_DIR}/onnxruntime.tgz"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )
        
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ONNX Runtime")
        endif()
        
        message(STATUS "Extracting ONNX Runtime...")
        file(ARCHIVE_EXTRACT
            INPUT "${CMAKE_BINARY_DIR}/onnxruntime.tgz"
            DESTINATION "${DEFAULT_DEPENDENCY_ROOT}"
        )
        
        file(REMOVE "${CMAKE_BINARY_DIR}/onnxruntime.tgz")
        message(STATUS "ONNX Runtime installed to ${ONNXRUNTIME_DIR}")
    else()
        message(STATUS "Using existing ONNX Runtime at ${ONNXRUNTIME_DIR}")
    endif()
endif()

set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_DIR}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")

if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIRS}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found in ${ONNXRUNTIME_INCLUDE_DIRS}")
endif()

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS ${ONNXRUNTIME_LIB_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(ONNXRUNTIME_GPU_LIBRARY
    NAMES onnxruntime_providers_cuda
    PATHS ${ONNXRUNTIME_LIB_DIR}
    NO_DEFAULT_PATH
)

if(ONNXRUNTIME_GPU_LIBRARY)
    message(STATUS "Found ONNX Runtime GPU: ${ONNXRUNTIME_GPU_LIBRARY}")
else()
    message(STATUS "ONNX Runtime GPU provider not found, using CPU only")
endif()

message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")

# Fetch object-detection-inference
FetchContent_Declare(
    object-detection-inference
    GIT_REPOSITORY https://github.com/olibartfast/object-detection-inference.git
    GIT_TAG        master
)
set(USE_GSTREAMER OFF)
set(DEFAULT_BACKEND "ONNX_RUNTIME")
FetchContent_MakeAvailable(object-detection-inference)
message(STATUS "object-detection-inference_SOURCE_DIR module path: ${object-detection-inference_SOURCE_DIR}")

# Fetch ByteTrack repository
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/trackers/ByteTrack)
set(BUILD_BYTETRACK_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ByteTrack
    GIT_REPOSITORY https://github.com/Vertical-Beach/ByteTrack-cpp.git
    GIT_TAG main
)
FetchContent_GetProperties(ByteTrack)
if(NOT ByteTrack_POPULATED)
    FetchContent_Populate(ByteTrack)
    add_subdirectory(${bytetrack_SOURCE_DIR} ${bytetrack_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
message(STATUS "ByteTrack: ${bytetrack_SOURCE_DIR}")

message(STATUS "Using system-installed Eigen3: ${EIGEN3_INCLUDE_DIR}")

message(STATUS "DEFAULT_BACKEND: ${DEFAULT_BACKEND}")
message(STATUS "USE_GSTREAMER: ${USE_GSTREAMER}")

# Set source files
set(SORT_SRC 
    trackers/SORT/Sort.cpp 
    trackers/SORT/KalmanTracker.cpp 
    trackers/SORT/Hungarian.cpp
)
file(GLOB_RECURSE BOTSORT_SRC "trackers/BoTSORT/src/*.cpp")

# Add the executable
add_executable(${PROJECT_NAME} 
    run.cpp 
    ${SORT_SRC} 
    ${BOTSORT_SRC}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
    include
    src
    ${object-detection-inference_SOURCE_DIR}/detectors/inc
    ${object-detection-inference_SOURCE_DIR}/detectors/src/
    ${object-detection-inference_SOURCE_DIR}/detectors/src/models   
    ${object-detection-inference_SOURCE_DIR}/common  
    ${GLOG_INCLUDE_DIRS}
    ${neuriplo_SOURCE_DIR}/backends
    ${neuriplo_SOURCE_DIR}/backends/src
    trackers/SORT
    trackers/BoTSORT/include
    ${bytetrack_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glog::glog
    ${OpenCV_LIBS}
    detectors
    neuriplo
    ${ONNXRUNTIME_LIBRARY}
    bytetrack
    Eigen3::Eigen
    -lstdc++
    -lpthread
    -ldl
)

# Link GPU library if available
if(ONNXRUNTIME_GPU_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_GPU_LIBRARY})
endif()
