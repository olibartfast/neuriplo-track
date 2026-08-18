# Test CMakeLists.txt
#
# Assertion-based test executables registered with ctest. No test framework is
# pulled in: the checks are simple enough not to justify a new dependency.

# Tracker behaviour: needs only the trackers library.
add_executable(test_trackers ${CMAKE_CURRENT_LIST_DIR}/test_trackers.cpp)
target_compile_options(test_trackers PRIVATE ${NEURIPLO_TRACK_WARNING_FLAGS})
target_include_directories(test_trackers PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(test_trackers PRIVATE trackers ${OpenCV_LIBS})
add_test(NAME trackers COMMAND test_trackers)

# Mask overlap primitive: also needs only the trackers library.
add_executable(test_mask_overlap ${CMAKE_CURRENT_LIST_DIR}/test_mask_overlap.cpp)
target_compile_options(test_mask_overlap PRIVATE ${NEURIPLO_TRACK_WARNING_FLAGS})
target_include_directories(test_mask_overlap PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(test_mask_overlap PRIVATE trackers ${OpenCV_LIBS})
add_test(NAME mask_overlap COMMAND test_mask_overlap)

# The remaining suites compile the application helpers directly, so they are
# skipped for a library-only build.
if(NOT BUILD_ONLY_LIB)
    add_executable(test_track_config
        ${CMAKE_CURRENT_LIST_DIR}/test_track_config.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../app/src/utils.cpp
    )
    target_compile_options(test_track_config PRIVATE ${NEURIPLO_TRACK_WARNING_FLAGS})
    target_include_directories(test_track_config PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../app/inc
        ${CMAKE_CURRENT_LIST_DIR}/../include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(test_track_config PRIVATE ${OpenCV_LIBS} glog::glog)
    add_test(NAME track_config COMMAND test_track_config)

    add_executable(test_utils
        ${CMAKE_CURRENT_LIST_DIR}/test_utils.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../app/src/utils.cpp
    )
    target_compile_options(test_utils PRIVATE ${NEURIPLO_TRACK_WARNING_FLAGS})
    target_include_directories(test_utils PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../app/inc
        ${CMAKE_CURRENT_LIST_DIR}/../include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(test_utils PRIVATE ${OpenCV_LIBS} glog::glog)
    add_test(NAME utils COMMAND test_utils)
endif()
