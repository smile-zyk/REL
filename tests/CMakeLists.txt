add_executable(test_scanner test_scanner.cc)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_scanner PRIVATE rel_core GTest::gtest_main)

add_executable(test_parser test_parser.cc)
target_link_libraries(test_parser PRIVATE rel_core GTest::gtest_main)

add_executable(test_environment test_environment.cc)
target_link_libraries(test_environment PRIVATE rel_core GTest::gtest_main)

add_executable(test_evaluator_lit test_evaluator_lit.cc)
target_link_libraries(test_evaluator_lit PRIVATE rel_core GTest::gtest_main)

add_executable(test_evaluator_gen test_evaluator_gen.cc)
target_link_libraries(test_evaluator_gen PRIVATE rel_core GTest::gtest_main)

add_executable(test_evaluator_op test_evaluator_op.cc)
target_link_libraries(test_evaluator_op PRIVATE rel_core GTest::gtest_main)

add_executable(test_evaluator_func test_evaluator_func.cc)
target_link_libraries(test_evaluator_func PRIVATE rel_core GTest::gtest_main)

add_executable(test_builtin_functions test_builtin_functions.cc)
target_link_libraries(test_builtin_functions PRIVATE rel_core GTest::gtest_main)

add_executable(test_math_functions test_math_functions.cc)
target_link_libraries(test_math_functions PRIVATE rel_core GTest::gtest_main)

add_executable(test_plugin test_plugin.cc)
target_link_libraries(test_plugin PRIVATE rel_core GTest::gtest_main)
target_compile_definitions(test_plugin PRIVATE
    REL_SAMPLE_PLUGIN_PATH="$<TARGET_FILE:rel_plugin_sample>")

# Copy runtime DLLs next to test binaries on Windows so gtest_discover_tests
# can find them during the build-time discovery step.
if(WIN32)
    foreach(_test_target test_environment test_evaluator_lit test_evaluator_gen test_evaluator_op test_evaluator_func test_builtin_functions test_plugin)
        add_custom_command(TARGET ${_test_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:${_test_target}>"
                "$<TARGET_FILE_DIR:${_test_target}>"
            COMMAND_EXPAND_LISTS
        )
    endforeach()
endif()

include(GoogleTest)
set(_gtest_discovery_timeout 60)
gtest_discover_tests(test_scanner DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_parser DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_environment DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_evaluator_lit DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_evaluator_gen DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_evaluator_op DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_evaluator_func DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_builtin_functions DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_math_functions DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
gtest_discover_tests(test_plugin DISCOVERY_TIMEOUT ${_gtest_discovery_timeout})
