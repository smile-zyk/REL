add_executable(test_scanner test_scanner.cc)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_scanner PRIVATE rel_core GTest::gtest_main)

add_executable(test_parser test_parser.cc)
target_link_libraries(test_parser PRIVATE rel_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_scanner)
gtest_discover_tests(test_parser)
