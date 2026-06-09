add_executable(test_scanner test_scanner.cc)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_scanner PRIVATE rel_scanner GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_scanner)
