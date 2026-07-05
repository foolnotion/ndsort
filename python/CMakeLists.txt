find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(_ext src/ndsort_bind.cpp)
target_link_libraries(_ext PRIVATE ndsort::ndsort)
target_compile_features(_ext PUBLIC cxx_std_20)

set_target_properties(_ext PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ndsort")

file(COPY ndsort/__init__.py DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/ndsort")

install(TARGETS _ext LIBRARY DESTINATION ndsort)
install(FILES ndsort/__init__.py DESTINATION ndsort)

# ── Python test suite (pytest) ────────────────────────────────────────────────
# Registers a CTest target that runs pytest with the built extension on PYTHONPATH.
# Skip with: ctest -E ndsort_pytest   or   cmake -DNDSORT_PYTHON_TESTS=OFF

option(NDSORT_PYTHON_TESTS "Register a CTest target for the Python pytest suite" ON)

if(NDSORT_PYTHON_TESTS)
    file(COPY
            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
            "${CMAKE_CURRENT_SOURCE_DIR}/pytest.ini"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

    add_test(
        NAME    ndsort_pytest
        COMMAND "${Python_EXECUTABLE}" -m pytest
                    --tb=short -q
                    "${CMAKE_CURRENT_BINARY_DIR}/tests"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_tests_properties(ndsort_pytest PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}"
        LABELS      "python"
    )
endif()
