cmake_minimum_required(VERSION 3.16)

project(REL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Test environment: load case/test_env.json at startup ----
add_compile_definitions(REL_LOAD_TEST_ENV)

# Third-party: xdataset (multi-dimensional data structures for REL).
# Enable HDF5 if it's available on the system.
if(DEFINED ENV{HDF5_ROOT} OR EXISTS "C:/msys64/mingw64/bin/libhdf5-320.dll")
    set(XDATASET_ENABLE_HDF5 ON CACHE BOOL "" FORCE)
endif()
# xdataset is a dependency here; its own unit tests should not pollute REL's
# CTest suite (it is pulled in with EXCLUDE_FROM_ALL, so xdataset_test is not
# built and gtest_discover_tests would register a NOT_BUILT stub).
set(BUILD_XDATASET_TEST OFF CACHE BOOL "Build xdataset unit tests" FORCE)
# REL does not need the pybind11 Python bindings (avoids requiring Python /
# pybind11 during configure).
set(BUILD_XDATASET_PYTHON OFF CACHE BOOL "Build the xdataset Python bindings" FORCE)
add_subdirectory(third_party/xdataset EXCLUDE_FROM_ALL)

# =========================================================================
#  rel_runtime (SHARED) — value layer + runtime services
# =========================================================================
#
#  Everything that operates on xdataset types without touching the REL
#  language front-end (scanner, parser, AST, evaluator).  Kept as a SHARED
#  library so Value/Function/Environment symbols can be consumed across DLL
#  boundaries (e.g. function plugins) with a single definition per type.
#
#  Contents:
#    value/              Value, Operation  (was rel_runtime)
#    eval/environment*   Environment, dataset/variable/function registries
#    eval/rel_plugin     dynamic library loader
#    eval/builtin/*      builtin function libraries
#    eval/function.h     Function / FunctionLibrary (header-only)

add_library(rel_runtime SHARED
    src/runtime/value.cc
    src/runtime/operation/operator.cc
    src/runtime/operation/pipeline.cc
    src/runtime/environment.cc
    src/runtime/environment_config.cc
    src/runtime/rel_plugin.cc
    src/runtime/builtin_library.cc
    src/runtime/math_library.cc
)
target_compile_definitions(rel_runtime PRIVATE REL_RUNTIME_BUILD_DLL)
target_include_directories(rel_runtime PUBLIC src/runtime)
target_link_libraries(rel_runtime PUBLIC xdataset)

# Plugin loader needs dlopen on Linux.
if(UNIX AND NOT APPLE)
    target_link_libraries(rel_runtime PUBLIC ${CMAKE_DL_LIBS})
endif()

# =========================================================================
#  rel_core (STATIC) — REL language front-end + interpreter
# =========================================================================
#
#  Scanner, parser, AST, and evaluator.  Depends on rel_runtime for Value
#  types and Environment services.

add_library(rel_core STATIC
    src/core/token.cc
    src/core/scanner.cc
    src/core/expr.cc
    src/core/ast_printer.cc
    src/core/parser.cc
    src/core/error.cc
    src/core/evaluator.cc
    src/core/rel.cc
)
target_include_directories(rel_core PUBLIC src/core src/runtime)
target_link_libraries(rel_core PUBLIC xdataset rel_runtime)

# =========================================================================
#  rel (EXECUTABLE) — CLI driver: REPL + file mode
# =========================================================================

add_executable(rel src/main.cc)
target_link_libraries(rel PRIVATE rel_core)

# Readline / editline for REPL history (up/down arrow navigation).
if(APPLE)
    find_library(EDIT_LIBRARY edit REQUIRED)
    target_link_libraries(rel PRIVATE ${EDIT_LIBRARY})
elseif(UNIX)
    find_package(Readline REQUIRED)
    target_link_libraries(rel PRIVATE Readline::Readline)
endif()

# Copy xdataset DLL, rel_runtime DLL and its dependencies next to rel.exe.
if(WIN32)
    add_custom_command(TARGET rel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:xdataset>"
            "$<TARGET_FILE_DIR:rel>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rel_runtime>"
            "$<TARGET_FILE_DIR:rel>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-mingw-dynamic/debug/bin/libunits.dll"
            "$<TARGET_FILE_DIR:rel>"
    )
    if(XDATASET_ENABLE_HDF5 AND HDF5_ROOT)
        add_custom_command(TARGET rel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HDF5_ROOT}/bin/libhdf5-320.dll"
                "$<TARGET_FILE_DIR:rel>"
        )
    endif()
endif()

# =========================================================================
#  Function plugin (sample)
# =========================================================================
#
#  A plugin only needs rel_runtime (function.h is header-only); it does not
#  link against rel_core.

add_library(rel_plugin_sample SHARED plugins/sample/rel_plugin_sample.cc)
target_include_directories(rel_plugin_sample PRIVATE src/runtime)
target_link_libraries(rel_plugin_sample PRIVATE rel_runtime)

# ---- Tests ---------------------------------------------------------------
# Single test executable (same pattern as xdataset_test): all test sources are
# compiled into one binary and registered with CTest via gtest_discover_tests.
enable_testing()
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(rel_test
    tests/test_scanner.cc
    tests/test_parser.cc
    tests/test_environment.cc
    tests/test_evaluator_lit.cc
    tests/test_evaluator_gen.cc
    tests/test_evaluator_op.cc
    tests/test_evaluator_func.cc
    tests/test_builtin_functions.cc
    tests/test_math_functions.cc
    tests/test_plugin.cc
    tests/value_test.cc
    tests/operation_test.cc
)
target_link_libraries(rel_test PRIVATE rel_core GTest::gtest_main)
target_compile_definitions(rel_test PRIVATE
    REL_SAMPLE_PLUGIN_PATH="$<TARGET_FILE:rel_plugin_sample>")

# Copy runtime DLLs next to the test binary on Windows so gtest_discover_tests
# can find them during the build-time discovery step.
if(WIN32)
    add_custom_command(TARGET rel_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:rel_test>"
            "$<TARGET_FILE_DIR:rel_test>"
        COMMAND_EXPAND_LISTS
    )
    if(XDATASET_ENABLE_HDF5 AND HDF5_ROOT)
        add_custom_command(TARGET rel_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HDF5_ROOT}/bin/libhdf5-320.dll"
                "$<TARGET_FILE_DIR:rel_test>"
        )
    endif()
endif()

set(_gtest_discovery_timeout 60)
gtest_discover_tests(rel_test DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
