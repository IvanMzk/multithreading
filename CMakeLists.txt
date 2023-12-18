cmake_minimum_required(VERSION 3.5)
project(multithreading)

set(include_dir ${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

option(BUILD_TEST "build multithreading tests" OFF)
option(BUILD_BENCHMARK "build multithreading benchmark" OFF)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(BUILD_TEST)
        add_subdirectory(test)
    endif()
    if(BUILD_BENCHMARK)
        add_subdirectory(benchmark)
    endif()
endif()

add_library(multithreading INTERFACE)
target_include_directories(multithreading INTERFACE ${include_dir})
target_compile_features(multithreading INTERFACE cxx_std_17)