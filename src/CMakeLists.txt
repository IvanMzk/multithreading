add_executable(Main)
target_include_directories(Main PRIVATE ${${project_name}_SOURCE_DIR}\\src)
target_compile_options(Main PRIVATE -W1 -std:c++17)

target_sources(Main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)