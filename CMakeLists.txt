cmake_minimum_required(VERSION 3.16)

project(REL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Third-party: xdataset (multi-dimensional data structures for REL).
add_subdirectory(third_party/xdataset EXCLUDE_FROM_ALL)

# Core library: scanner, parser, AST, evaluator (linked by both the CLI and
# the test executable).
add_library(rel_core STATIC
    src/scanner/token.cc
    src/scanner/scanner.cc
    src/ast/expr.cc
    src/ast/ast_printer.cc
    src/parser/parser.cc
    src/eval/value.cc
    src/eval/environment.cc
)
target_include_directories(rel_core PUBLIC src)
target_link_libraries(rel_core PUBLIC xdataset)

# CLI driver: REPL + file mode.
add_executable(rel src/main.cc)
target_link_libraries(rel PRIVATE rel_core)

# Tests.
enable_testing()
add_subdirectory(tests)
