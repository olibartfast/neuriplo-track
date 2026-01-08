# Version management for vision-tracking project dependencies
# This file centralizes all version information for external dependencies

# Function to read versions from .env file
function(read_versions_from_env)
    set(ENV_FILE "${CMAKE_SOURCE_DIR}/versions.env")
    if(NOT EXISTS "${ENV_FILE}")
        message(WARNING "versions.env file not found at ${ENV_FILE}")
        return()
    endif()
    
    file(STRINGS "${ENV_FILE}" ENV_LINES)
    foreach(LINE ${ENV_LINES})
        # Skip empty lines and comments
        if(LINE MATCHES "^[ \t]*#" OR LINE STREQUAL "")
            continue()
        endif()
        
        # Parse KEY=VALUE pairs
        if(LINE MATCHES "^([A-Za-z_][A-Za-z0-9_]*)=(.*)$")
            set(VAR_NAME "${CMAKE_MATCH_1}")
            set(VAR_VALUE "${CMAKE_MATCH_2}")
            # Remove quotes if present
            if(VAR_VALUE MATCHES "^\"(.*)\"$")
                string(REGEX REPLACE "^\"(.*)\"$" "\\1" VAR_VALUE "${VAR_VALUE}")
            endif()
            # Set the variable
            set(${VAR_NAME} "${VAR_VALUE}" PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# Read versions from the .env file
read_versions_from_env()

# External C++ Libraries (fetched via CMake FetchContent)
set(BYTETRACK_VERSION ${BYTETRACK_VERSION} CACHE STRING "ByteTrack library version")
set(VISION_CORE_VERSION ${VISION_CORE_VERSION} CACHE STRING "vision-core library version")
set(NEURIPLO_VERSION ${NEURIPLO_VERSION} CACHE STRING "neuriplo library version")

# System Dependencies (minimum versions)
set(OPENCV_MIN_VERSION ${OPENCV_MIN_VERSION} CACHE STRING "Minimum OpenCV version")
set(GLOG_MIN_VERSION ${GLOG_MIN_VERSION} CACHE STRING "Minimum glog version")
set(EIGEN3_MIN_VERSION ${EIGEN3_MIN_VERSION} CACHE STRING "Minimum Eigen3 version")
set(CMAKE_MIN_VERSION ${CMAKE_MIN_VERSION} CACHE STRING "Minimum CMake version")

# Print version information for debugging
message(STATUS "=== Project Dependency Versions ===")
message(STATUS "ByteTrack: ${BYTETRACK_VERSION}")
message(STATUS "vision-core: ${VISION_CORE_VERSION}")
message(STATUS "neuriplo: ${NEURIPLO_VERSION}")
message(STATUS "OpenCV (minimum): ${OPENCV_MIN_VERSION}")
message(STATUS "glog (minimum): ${GLOG_MIN_VERSION}")
message(STATUS "Eigen3 (minimum): ${EIGEN3_MIN_VERSION}")
message(STATUS "CMake (minimum): ${CMAKE_MIN_VERSION}")
message(STATUS "====================================")
