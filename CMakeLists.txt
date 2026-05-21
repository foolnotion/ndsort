cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    ndsort
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    ndsort_ndsort
    source/ndsort.cpp
)
add_library(ndsort::ndsort ALIAS ndsort_ndsort)

include(GenerateExportHeader)
generate_export_header(
    ndsort_ndsort
    BASE_NAME ndsort
    EXPORT_FILE_NAME export/ndsort/ndsort_export.hpp
    CUSTOM_CONTENT_FROM_VARIABLE pragma_suppress_c4251
)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(ndsort_ndsort PUBLIC NDSORT_STATIC_DEFINE)
endif()

set_target_properties(
    ndsort_ndsort PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    EXPORT_NAME ndsort
    OUTPUT_NAME ndsort
)

target_include_directories(
    ndsort_ndsort ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_include_directories(
    ndsort_ndsort SYSTEM
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/export>"
)

target_compile_features(ndsort_ndsort PUBLIC cxx_std_20)

find_package(fmt REQUIRED)
target_link_libraries(ndsort_ndsort PRIVATE fmt::fmt)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Examples ----

if(PROJECT_IS_TOP_LEVEL)
  option(BUILD_EXAMPLES "Build examples tree." "${ndsort_DEVELOPER_MODE}")
  if(BUILD_EXAMPLES)
    add_subdirectory(example)
  endif()
endif()

# ---- Developer mode ----

if(NOT ndsort_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of ndsort"
  )
endif()

include(cmake/dev-mode.cmake)
