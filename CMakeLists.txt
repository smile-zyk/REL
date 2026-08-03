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
add_subdirectory(third_party/xdataset EXCLUDE_FROM_ALL)

# Core library: scanner, parser, AST, evaluator (linked by both the CLI and
# the test executable).
add_library(rel_core STATIC
    src/scanner/token.cc
    src/scanner/scanner.cc
    src/ast/expr.cc
    src/ast/ast_printer.cc
    src/parser/parser.cc
    src/eval/environment.cc
    src/eval/environment_config.cc
    src/eval/error.cc
    src/eval/evaluator.cc
    src/eval/function.cc
    src/eval/builtin_functions.cc
    src/rel.cc
)
target_include_directories(rel_core PUBLIC src)
target_link_libraries(rel_core PUBLIC xdataset)

# CLI driver: REPL + file mode.
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

# Copy xdataset DLL and its dependencies next to rel.exe on Windows.
if(WIN32)
    add_custom_command(TARGET rel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:xdataset>"
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

# Tests.
enable_testing()
add_subdirectory(tests)
