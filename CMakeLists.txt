cmake_minimum_required(VERSION 3.14)
project(Arena CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1  # Use latest stable version
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest tests" FORCE)

FetchContent_MakeAvailable(googlebenchmark)

add_library(arena INTERFACE)
target_include_directories(arena INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(arena INTERFACE cxx_std_23)

add_executable(arena_benchmark
        benchmarks/benchmark.cpp
)

target_link_libraries(arena_benchmark
        PRIVATE
        arena
        benchmark::benchmark
        benchmark::benchmark_main
)

target_compile_options(arena_benchmark PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
)
