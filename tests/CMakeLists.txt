add_executable(test_scanner test_scanner.cc)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_scanner PRIVATE rel_core GTest::gtest_main)

add_executable(test_parser test_parser.cc)
target_link_libraries(test_parser PRIVATE rel_core GTest::gtest_main)

add_executable(test_value test_value.cc)
target_link_libraries(test_value PRIVATE rel_core GTest::gtest_main)

add_executable(test_environment test_environment.cc)
target_link_libraries(test_environment PRIVATE rel_core GTest::gtest_main)

add_executable(test_evaluator_lit test_evaluator_lit.cc)
target_link_libraries(test_evaluator_lit PRIVATE rel_core GTest::gtest_main)

add_executable(test_evaluator_gen test_evaluator_gen.cc)
target_link_libraries(test_evaluator_gen PRIVATE rel_core GTest::gtest_main)

# Copy runtime DLLs next to test binaries on Windows so gtest_discover_tests
# can find them during the build-time discovery step.
if(WIN32)
    foreach(_test_target test_value test_environment test_evaluator_lit test_evaluator_gen)
        add_custom_command(TARGET ${_test_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:${_test_target}>"
                "$<TARGET_FILE_DIR:${_test_target}>"
            COMMAND_EXPAND_LISTS
        )
    endforeach()
endif()

include(GoogleTest)
gtest_discover_tests(test_scanner)
gtest_discover_tests(test_parser)
gtest_discover_tests(test_value)
gtest_discover_tests(test_environment)
gtest_discover_tests(test_evaluator_lit)
gtest_discover_tests(test_evaluator_gen)
