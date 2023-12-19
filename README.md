## Multithreading components

### Single thread bounded queue

Ring buffer implementation.

### Single thread unbounded queue of polymorphic objects

Store polymorphic objects of dynamic types that are derived from some base type, using pointers to base type.

`push` member function should be explicitly specialized with dynamic type to construct.
It uses single allocation for queue control structures and constructed object.

### MPMC bounded queues

Multiple producers, multiple consumers ring buffers implementations with and without mutexes.

### Reusable resources pool

Multiple consumers bounded pool of reusable resources.
Its main purpose to reuse resourses that are expensive to create each time its needed.
e.g. locked (pined) memory buffers

### Thread pools

Fixed signature thread pool, allocation free.

Arbitrary signature thread pool, single allocation per task.

## Including into project

All components are header only so to use its in project you can include needed header files and everything will work.

Also you can utilize `add_subdirectory(...)` CMake command, e.g.:

```cmake
cmake_minimum_required(VERSION 3.5)
project(my_project)
add_subdirectory(path_to_multithreading_dir multithreading)
add_executable(my_target)
target_link_libraries(my_target PRIVATE multithreading)
...
```

## Build tests and benchmarks

[Catch](https://github.com/catchorg/Catch2) framework is used for testing.

To build and run tests:

```cmake
cmake -B build_dir -DBUILD_TEST=ON
cmake --build build_dir
build_dir/test/Test
```

To build and run benchmarks:

```cmake
cmake -B build_dir -DBUILD_BENCHMARK=ON
cmake --build build_dir
build_dir/benchmark/Benchmark
```

## License
This software is licensed under the [BSL 1.0](LICENSE.txt).