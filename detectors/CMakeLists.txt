# Placeholder CMakeLists.txt for detectors directory
# This is a workaround for object-detection-inference CMakeLists.txt structure
# The actual detectors library comes from the fetched dependency

# Empty CMakeLists to satisfy add_subdirectory() call
# Placeholder CMakeLists.txt for detectors directory
# This includes the actual detectors sources from object-detection-inference

message(STATUS "Building detectors from object-detection-inference dependency")

# Get the source directory for the fetched dependency
set(DETECTORS_SRC_DIR ${CMAKE_SOURCE_DIR}/build/_deps/object-detection-inference-src/detectors)

# Include the actual detectors CMakeLists from the fetched dependency
if(EXISTS ${DETECTORS_SRC_DIR}/CMakeLists.txt)
    add_subdirectory(${DETECTORS_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/detectors_actual)
else()
    message(FATAL_ERROR "Detectors CMakeLists.txt not found at ${DETECTORS_SRC_DIR}")
endif()

