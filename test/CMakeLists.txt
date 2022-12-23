add_executable(Test)
add_executable(Test_selected)
target_include_directories(Test PRIVATE ${${project_name}_SOURCE_DIR}\\src)
target_compile_options(Test PRIVATE -W1 -std:c++17)
target_include_directories(Test_selected PRIVATE ${${project_name}_SOURCE_DIR}\\src)
target_compile_options(Test_selected PRIVATE -W1 -std:c++17)
target_link_options(Test_selected PRIVATE /STACK:104857600)

target_sources(Test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark_static_stack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_static_stack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_static_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark_circular_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_circular_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark_mpmc_bounded_queue.cpp
)

target_sources(Test_selected PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mpmc_bounded_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
)