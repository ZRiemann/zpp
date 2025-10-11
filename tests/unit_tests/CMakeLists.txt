add_executable(gtest_folly
    TestMain.cpp
    benchmark_cpu_executor.cpp
    folly_coro_test.cpp
    folly_coro_AsyncScope.cpp
)

target_link_libraries(gtest_folly PRIVATE
    cxx_options
    zpp::folly
    GTest::gtest
)

add_executable(gtest_tsc
    timestamp_counter_test.cpp
    timestamp_stc_test.cpp
    timestamp_chorno_test.cpp
)

target_link_libraries(gtest_tsc PRIVATE
    cxx_options
    zpp::zpp
    GTest::gtest_main
)

target_include_directories(gtest_tsc PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

include(GoogleTest)

gtest_discover_tests(gtest_tsc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DISCOVERY_TIMEOUT 30
)

gtest_discover_tests(gtest_folly
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DISCOVERY_TIMEOUT 30
)
# Add run target for the test
add_run_target(gtest_tsc)
add_run_target(gtest_folly ARGS --folly_global_cpu_executor_threads=16 --folly_global_io_executor_threads=4)
