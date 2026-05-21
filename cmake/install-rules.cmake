if(PROJECT_IS_TOP_LEVEL)
  set(
      CMAKE_INSTALL_INCLUDEDIR "include/ndsort-${PROJECT_VERSION}"
      CACHE STRING ""
  )
  set_property(CACHE CMAKE_INSTALL_INCLUDEDIR PROPERTY TYPE PATH)
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
# should match the name of variable set in the install-config.cmake script
set(package ndsort)

install(
    DIRECTORY
    include/
    "${PROJECT_BINARY_DIR}/export/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT ndsort_Development
)

install(
    TARGETS ndsort_ndsort
    EXPORT ndsortTargets
    RUNTIME #
    COMPONENT ndsort_Runtime
    LIBRARY #
    COMPONENT ndsort_Runtime
    NAMELINK_COMPONENT ndsort_Development
    ARCHIVE #
    COMPONENT ndsort_Development
    INCLUDES #
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    ndsort_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${package}"
    CACHE STRING "CMake package config location relative to the install prefix"
)
set_property(CACHE ndsort_INSTALL_CMAKEDIR PROPERTY TYPE PATH)
mark_as_advanced(ndsort_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${ndsort_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT ndsort_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${ndsort_INSTALL_CMAKEDIR}"
    COMPONENT ndsort_Development
)

install(
    EXPORT ndsortTargets
    NAMESPACE ndsort::
    DESTINATION "${ndsort_INSTALL_CMAKEDIR}"
    COMPONENT ndsort_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
