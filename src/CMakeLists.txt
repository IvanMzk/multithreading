add_library(multithreading INTERFACE)
target_include_directories(multithreading INTERFACE ${${project_name}_SOURCE_DIR}\\src)
target_compile_options(multithreading INTERFACE -W1 -std:c++17)