add_executable(Test)
target_include_directories(Test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../qa_common)
target_link_libraries(Test PRIVATE multithreading)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(Test PRIVATE /W4 /Zc:__cplusplus "$<$<CONFIG:RELEASE>:/O2>")
else()
    target_compile_options(Test PRIVATE -Werror "$<$<CONFIG:RELEASE>:-O2>")
endif()

target_sources(Test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_queue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_bounded_pool.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_thread_pool.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_thread_pool_v3.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test.cpp
)