cmake_minimum_required(VERSION 3.20)
project(vision-tracking)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json to make it easier to work with clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Fetch external dependencies and runtime requirements
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

# Provide a consistent variable name for downstream targets
set(ONNX_RUNTIME_DIR ${ONNXRUNTIME_DIR} CACHE PATH "ONNX Runtime root directory")

# Fetch vision-core
FetchContent_Declare(
    vision-core
    GIT_REPOSITORY https://github.com/olibartfast/vision-core.git
    GIT_TAG        ${VISION_CORE_VERSION}
)
FetchContent_MakeAvailable(vision-core)
message(STATUS "vision-core_SOURCE_DIR: ${vision-core_SOURCE_DIR}")

# Fetch neuriplo
FetchContent_Declare(
    neuriplo
    GIT_REPOSITORY https://github.com/olibartfast/neuriplo.git
    GIT_TAG        ${NEURIPLO_VERSION}
)
FetchContent_MakeAvailable(neuriplo)
message(STATUS "neuriplo_SOURCE_DIR: ${neuriplo_SOURCE_DIR}")

# Fetch ByteTrack with specific version
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
