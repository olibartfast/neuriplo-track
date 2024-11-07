cmake_minimum_required(VERSION 3.20)
project(multi_object_tracking LANGUAGES CXX)
SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Use system-installed Eigen
find_package(Eigen3  REQUIRED)

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



# Confirm paths for Eigen
message(STATUS "Using system-installed Eigen3: ${EIGEN3_INCLUDE_DIR}")

# Set source files for YOLOv5 and SORT
set(YOLOV5_SRC detectors/YoloV5/YoloV5.cpp)
set(SORT_SRC trackers/SORT/Sort.cpp trackers/SORT/KalmanTracker.cpp trackers/SORT/Hungarian.cpp)

file(GLOB_RECURSE BOTSORT_SRC "trackers/BoTSORT/src/*.cpp")

# Find OpenCV
find_package(OpenCV REQUIRED)

# Add the executable for the main application
add_executable(${PROJECT_NAME} run.cpp ${YOLOV5_SRC} ${SORT_SRC} ${BOTSORT_SRC})

# Include directories for OpenCV, ByteTrack, Eigen, and project-specific headers
target_include_directories(${PROJECT_NAME} PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  include
  src
  detectors
  trackers/SORT
  trackers/BoTSORT/include
  ${bytetrack_SOURCE_DIR}/include
  ${EIGEN3_INCLUDE_DIR}
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
