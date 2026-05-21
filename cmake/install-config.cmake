set(ndsort_FOUND YES)

include(CMakeFindDependencyMacro)
find_dependency(fmt)

if(ndsort_FOUND)
  include("${CMAKE_CURRENT_LIST_DIR}/ndsortTargets.cmake")
endif()
