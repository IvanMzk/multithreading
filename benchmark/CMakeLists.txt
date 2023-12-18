add_executable(Benchmark)
target_include_directories(Benchmark PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../qa_common)
target_link_libraries(Benchmark PRIVATE multithreading)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(Benchmark PRIVATE /W4 /Zc:__cplusplus "$<$<CONFIG:RELEASE>:/O2>")
    target_link_options(Benchmark PRIVATE /STACK:8388608)
else()
    target_compile_options(Benchmark PRIVATE -Werror "$<$<CONFIG:RELEASE>:-O2>")
    target_link_options(Benchmark PRIVATE -Wl,--stack,8388608)
endif()

target_sources(Benchmark PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/benchmark_mpmc_bounded_queue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/benchmark.cpp
)