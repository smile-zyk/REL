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
#  Python plugin (embedded interpreter + pybind11 bindings)
# =========================================================================
#
#  BUILD_PYTHON controls whether the Python bridge is compiled into
#  rel_runtime.  On by default; set -DBUILD_PYTHON=OFF for a rel_runtime with
#  zero external dependencies (only xdataset).  When ON, it additionally
#  requires pybind11 (and its matching CPython).  numpy interop is implemented
#  via the buffer protocol, so numpy headers are NOT required at build time.
option(BUILD_PYTHON "Enable embedded Python + plugin support" ON)

# ---- Python bridge sources + pybind11 discovery ------------------------
#
#  The Python sources are listed here so they can appear in
#  compile_commands.json (for clangd) even when BUILD_PYTHON=OFF; they are
#  compiled into rel_runtime only when BUILD_PYTHON=ON.
set(REL_PYTHON_SOURCES
    src/runtime/python/rel_module.cc
    src/runtime/python/xdataset_bindings.cc
    src/runtime/python/rel_bindings.cc
    src/runtime/python/python_loader.cc
)

# Discover CPython first (before pybind11) so pybind11 reuses the found
# Python3::Python target instead of running its own discovery.  This is what
# makes `pybind11::embed` link against libpython and expose Python.h.
#   - BUILD_PYTHON=ON  -> REQUIRED (embedding needs the interpreter + libpython)
#   - BUILD_PYTHON=OFF -> QUIET best-effort (only to feed the clangd index below)
if(BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
else()
    find_package(Python3 COMPONENTS Interpreter Development.Embed QUIET)
endif()

# Best-effort pybind11 discovery (required only when BUILD_PYTHON=ON).  This
# also locates a pip-installed pybind11 via `python3 -m pybind11 --cmakedir`,
# so BUILD_PYTHON=ON does not need a manually supplied pybind11_DIR.
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND AND Python3_Interpreter_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_VARIABLE _pybind11_cmakedir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_pybind11_cmakedir)
        set(pybind11_DIR "${_pybind11_cmakedir}")
        find_package(pybind11 CONFIG QUIET)
    endif()
endif()

# =========================================================================
#  rel_runtime (SHARED) — value layer + runtime services
# =========================================================================
#
#  Everything that operates on xdataset types without touching the REL
#  language front-end (scanner, parser, AST, evaluator).  Kept as a SHARED
#  library so Value/Function/Environment symbols can be consumed by C++
#  extensions (static libraries) and the Python bridge with a single
#  definition per type.
#
#  Contents:
#    value.*             Value
#    environment.*       Environment, dataset/variable/function registries
#    function/*          Function / FunctionLibrary (header-only)
#    builtin_library/*   "builtin" function library
#    math_library/*      "math" function library
#    operation/*         Value operators + math operations

set(REL_RUNTIME_SOURCES
    src/runtime/value.cc
    src/runtime/function/function.cc
    src/runtime/builtin_library/math_library.cc
    src/runtime/builtin_library/builtin_library.cc
    src/runtime/operation/operator.cc
    src/runtime/operation/pipeline.cc
    src/runtime/operation/operation_helpers.cc
    src/runtime/operation/math_operation.cc
    src/runtime/environment.cc
    src/runtime/environment_config.cc
)

if(BUILD_PYTHON)
    if(NOT pybind11_FOUND)
        message(FATAL_ERROR
            "BUILD_PYTHON=ON requires pybind11. Install it (e.g. pip install "
            "pybind11) or point CMake at it via -Dpybind11_DIR=...")
    endif()
    list(APPEND REL_RUNTIME_SOURCES ${REL_PYTHON_SOURCES})
endif()

add_library(rel_runtime SHARED ${REL_RUNTIME_SOURCES})
target_compile_definitions(rel_runtime PRIVATE REL_RUNTIME_BUILD_DLL)
target_include_directories(rel_runtime PUBLIC src/runtime)
target_link_libraries(rel_runtime PUBLIC xdataset)

if(BUILD_PYTHON)
    # The whole project is C++11 (see CMAKE_CXX_STANDARD above); the Python
    # bridge stays C++11 too — the buffer-protocol approach avoids the
    # C++14+ pybind11 helpers (no overload_cast / generic lambdas).
    target_compile_definitions(rel_runtime PRIVATE REL_HAS_PYTHON)
    target_link_libraries(rel_runtime PRIVATE pybind11::embed)
    # pybind11's quiet discovery can fall back to the legacy FindPythonLibs,
    # which sets PYTHON_INCLUDE_DIRS but not Python3_INCLUDE_DIRS — the latter
    # is what pybind11::python_headers expects.  Add the CPython include path
    # explicitly so rel_runtime sees Python.h.
    target_include_directories(rel_runtime PRIVATE ${Python3_INCLUDE_DIRS})
elseif(pybind11_FOUND)
    # BUILD_PYTHON=OFF: still emit compile_commands.json entries for the
    # Python sources so clangd can index them against pybind11 + CPython
    # headers.  This OBJECT library is never built (EXCLUDE_FROM_ALL), but its
    # compile flags (include paths + REL_HAS_PYTHON) land in
    # compile_commands.json.
    add_library(rel_python_index OBJECT EXCLUDE_FROM_ALL ${REL_PYTHON_SOURCES})
    target_compile_definitions(rel_python_index PRIVATE
        REL_HAS_PYTHON REL_RUNTIME_BUILD_DLL)
    target_include_directories(rel_python_index PRIVATE
        src/runtime ${Python3_INCLUDE_DIRS})
    target_link_libraries(rel_python_index PRIVATE xdataset pybind11::headers)
endif()

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
target_link_libraries(rel PRIVATE rel_core rel_function_library_sample)

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
#  Function library (sample static extension)
# =========================================================================
#
#  A C++ extension is a static library that links rel_runtime and exposes a
#  MakeLibrary() factory.  The host (rel.exe / tests) registers it explicitly
#  via Environment::RegisterLibrary.  It does not link rel_core.

add_library(rel_function_library_sample STATIC
    src/function_library/function_library_sample.cc
)
target_include_directories(rel_function_library_sample PUBLIC
    src/function_library
    src/runtime
)
target_link_libraries(rel_function_library_sample PUBLIC rel_runtime)

# ---- Tests ---------------------------------------------------------------
# Single test executable (same pattern as xdataset_test): all test sources are
# compiled into one binary and registered with CTest via gtest_discover_tests.
enable_testing()
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(REL_TEST_SOURCES
    tests/test_scanner.cc
    tests/test_parser.cc
    tests/test_environment.cc
    tests/test_evaluator_lit.cc
    tests/test_evaluator_gen.cc
    tests/test_evaluator_op.cc
    tests/test_evaluator_func.cc
    tests/test_builtin_functions.cc
    tests/test_math_operation.cc
    tests/test_function_library.cc
    tests/value_test.cc
    tests/operation_test.cc
)

# Python plugin tests need the embedded interpreter, so they are only built
# when BUILD_PYTHON=ON.
if(BUILD_PYTHON)
    list(APPEND REL_TEST_SOURCES tests/test_python_plugin.cc)
endif()

add_executable(rel_test ${REL_TEST_SOURCES})
target_link_libraries(rel_test PRIVATE rel_core rel_function_library_sample GTest::gtest_main)

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
