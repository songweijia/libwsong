cmake_minimum_required(VERSION 3.12.4)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

add_library(perf_objs OBJECT
    timing.cpp)
target_include_directories(perf_objs PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
