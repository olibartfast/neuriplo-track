# Dependency validation and setup utilities for vision-tracking project
# This module provides functions to validate and setup dependencies

include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

# Function to validate a dependency exists
function(validate_dependency dependency_name dependency_path)
    if(NOT EXISTS "${dependency_path}")
        message(FATAL_ERROR "${dependency_name} not found at ${dependency_path}. 
        Please ensure the dependency is properly installed or run the setup script.")
    endif()
    
    message(STATUS "✓ ${dependency_name} found at ${dependency_path}")
endfunction()

# Function to validate system dependencies
function(validate_system_dependencies)
    # OpenCV, glog, and Eigen3 should already be found before this function is called
    if(NOT OpenCV_FOUND)
        find_package(OpenCV REQUIRED)
    endif()
    if(OpenCV_VERSION VERSION_LESS OPENCV_MIN_VERSION)
        message(FATAL_ERROR "OpenCV version ${OpenCV_VERSION} is too old. Minimum required: ${OPENCV_MIN_VERSION}")
    endif()
    message(STATUS "✓ OpenCV ${OpenCV_VERSION} found")
    
    if(NOT glog_FOUND)
        find_package(glog REQUIRED)
    endif()
    message(STATUS "✓ glog found")
    
    if(NOT Eigen3_FOUND)
        find_package(Eigen3 REQUIRED)
    endif()
    message(STATUS "✓ Eigen3 found")
endfunction()

# Function to validate fetched dependencies
function(validate_fetched_dependencies)
    # Validate object-detection-inference library
    if(NOT DEFINED object-detection-inference_SOURCE_DIR)
        message(FATAL_ERROR "object-detection-inference library not found. This should be fetched automatically by CMake.")
    endif()
    message(STATUS "✓ object-detection-inference library found at ${object-detection-inference_SOURCE_DIR}")
    
    # Validate ByteTrack library
    if(NOT DEFINED bytetrack_SOURCE_DIR)
        message(FATAL_ERROR "ByteTrack library not found. This should be fetched automatically by CMake.")
    endif()
    message(STATUS "✓ ByteTrack library found at ${bytetrack_SOURCE_DIR}")
endfunction()

# Function to validate all dependencies for this project
function(validate_all_dependencies)
    message(STATUS "=== Validating Project Dependencies ===")
    
    validate_system_dependencies()
    validate_fetched_dependencies()
    
    message(STATUS "=== All Project Dependencies Validated Successfully ===")
endfunction()

# Function to check if we're in a Docker environment
function(is_docker_environment result)
    if(EXISTS "/.dockerenv")
        set(${result} TRUE PARENT_SCOPE)
    else()
        set(${result} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function to provide helpful setup instructions
function(print_setup_instructions)
    message(STATUS "=== Setup Instructions ===")
    message(STATUS "This project uses the object-detection-inference library for object detection.")
    message(STATUS "System dependencies can be installed with:")
    message(STATUS "  sudo apt update && sudo apt install -y libopencv-dev libgoogle-glog-dev libeigen3-dev")
    message(STATUS "")
endfunction()
