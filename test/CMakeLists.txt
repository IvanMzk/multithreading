add_executable(Test)
target_link_libraries(Test PRIVATE multithreading)
target_compile_features(Test PRIVATE cxx_std_17)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(Test PRIVATE /W4 /Zc:__cplusplus "$<$<CONFIG:RELEASE>:/O2>")
    target_link_options(Test PRIVATE /STACK:104857600)
else()
    target_compile_options(Test PRIVATE -Werror "$<$<CONFIG:RELEASE>:-O3>")
    target_link_options(Test PRIVATE -Wl,--stack,104857600)
endif()
target_sources(Test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bounded_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_pool_v3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
)