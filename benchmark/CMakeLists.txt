cmake_minimum_required(VERSION 3.14)

project(ndsortBenchmarks LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_package(ndsort REQUIRED)
endif()

find_package(nanobench REQUIRED)

# ---- Benchmarks ----

add_executable(ndsort_bench source/bench.cpp)
target_link_libraries(
    ndsort_bench PRIVATE
    ndsort::ndsort
    nanobench::nanobench
)
target_compile_features(ndsort_bench PRIVATE cxx_std_20)
target_compile_definitions(
    ndsort_bench PRIVATE
    NDSORT_DATA_DIR="${CMAKE_SOURCE_DIR}/test/data"
)

# ---- End-of-file commands ----

add_folders(Benchmark)
