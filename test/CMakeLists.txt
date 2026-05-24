cmake_minimum_required(VERSION 3.14)

project(ndsortTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
  find_package(ndsort REQUIRED)
  enable_testing()
endif()

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(ndsort_test source/ndsort_test.cpp)
target_link_libraries(
    ndsort_test PRIVATE
    ndsort::ndsort
    Catch2::Catch2WithMain
)
target_compile_features(ndsort_test PRIVATE cxx_std_20)

add_test(NAME ndsort_test COMMAND ndsort_test)

# ---- End-of-file commands ----

add_folders(Test)
