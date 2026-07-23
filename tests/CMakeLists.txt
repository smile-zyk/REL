add_executable(test_scanner test_scanner.cc)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_scanner PRIVATE rel_core GTest::gtest_main)

add_executable(test_parser test_parser.cc)
target_link_libraries(test_parser PRIVATE rel_core GTest::gtest_main)

add_executable(test_value test_value.cc)
target_link_libraries(test_value PRIVATE rel_core GTest::gtest_main)
# Copy all runtime DLLs next to the test binary so gtest_discover_tests
# can find them during the build-time discovery step (required on MinGW).
add_custom_command(TARGET test_value POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:test_value>"
        "$<TARGET_FILE_DIR:test_value>"
    COMMAND_EXPAND_LISTS
)

include(GoogleTest)
gtest_discover_tests(test_scanner)
gtest_discover_tests(test_parser)
gtest_discover_tests(test_value)
