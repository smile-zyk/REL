cmake_minimum_required(VERSION 3.16)

project(REL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Scanner library (linked by both the CLI and the test executable).
add_library(rel_scanner STATIC
    src/scanner/token.cc
    src/scanner/scanner.cc
)
target_include_directories(rel_scanner PUBLIC src)

# CLI driver: REPL + file mode.
add_executable(rel src/main.cc)
target_link_libraries(rel PRIVATE rel_scanner)

# Tests.
enable_testing()
add_subdirectory(tests)
