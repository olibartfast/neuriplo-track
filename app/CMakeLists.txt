# App CMakeLists.txt for Multi-Object Tracking

# Define sources
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MultiObjectTrackingApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CommandLineParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../trackers
    ${CMAKE_CURRENT_SOURCE_DIR}/../trackers/SORT
    ${CMAKE_CURRENT_SOURCE_DIR}/../trackers/BoTSORT/include
    ${neuriplo_SOURCE_DIR}/include
    ${neuriplo_SOURCE_DIR}/src
    ${neuriplo_SOURCE_DIR}/backends
    ${neuriplo_SOURCE_DIR}/backends/src
    ${bytetrack_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glog::glog
    ${OpenCV_LIBS}
    vision-core::vision-core
    neuriplo
    trackers
    Eigen3::Eigen
)
