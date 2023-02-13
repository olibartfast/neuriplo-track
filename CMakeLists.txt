
cmake_minimum_required(VERSION 3.5)
project(yoloSort LANGUAGES CXX)
SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YOLO_SORT_SRC
        run.cpp YoloV5.cpp Sort.cpp KalmanTracker.cpp Hungarian.cpp)

find_package(OpenCV)


add_executable(${PROJECT_NAME}  ${YOLO_SORT_SRC})

target_include_directories(${PROJECT_NAME}  PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}   PRIVATE ${OpenCV_LIBRARIES} -lstdc++ -lpthread -ldl)