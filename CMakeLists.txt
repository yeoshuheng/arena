cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

add_executable(arena src/main.cpp
        include/allocator.h
        benchmark/benchmark.cpp
        benchmark/utils.h)

target_compile_features(arena PRIVATE cxx_std_23)

set_target_properties(arena PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)
