# Trackers library CMakeLists.txt

# Set source files for SORT
set(SORT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/SORT/Sort.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SORT/KalmanTracker.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SORT/Hungarian.cpp
)

# Set source files for BoTSORT
file(GLOB_RECURSE BOTSORT_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BoTSORT/src/*.cpp")

# Set wrapper sources
set(WRAPPER_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/SortWrapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/BoTSORTWrapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ByteTrackWrapper.cpp
)

# Create the trackers library
add_library(trackers SHARED 
    ${SORT_SOURCES}
    ${BOTSORT_SOURCES}
    ${WRAPPER_SOURCES}
)

message(STATUS "Trackers Module, CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message(STATUS "Trackers Module, bytetrack_SOURCE_DIR: ${bytetrack_SOURCE_DIR}")

# Include directories
target_include_directories(trackers PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/SORT
    ${CMAKE_CURRENT_LIST_DIR}/BoTSORT/include
    ${bytetrack_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${ONNX_RUNTIME_DIR}/include
)

# Link libraries
target_link_libraries(trackers PUBLIC
    ${OpenCV_LIBS}
    glog::glog
    bytetrack
    Eigen3::Eigen
    ${ONNX_RUNTIME_DIR}/lib/libonnxruntime.so
    vision-core::vision-core
)
