add_executable(gtest_tsc
    timestamp_counter_test.cpp
    timestamp_stc_test.cpp
    timestamp_chorno_test.cpp
    timer_test.cpp
    wall_time_test.cpp
    wall_time_legacy_test.cpp
    json_hpp_test.cpp
    hpx_server_lifecycle_test.cpp
    logging_test.cpp
    logging_active_level_test.cpp
    stl_queue_test.cpp
)

set_source_files_properties(wall_time_legacy_test.cpp PROPERTIES
    COMPILE_DEFINITIONS ZPP_WALL_TIME_FORCE_LEGACY=1
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

# Add run target for the test
add_run_target(gtest_tsc)

if(TARGET zpp::zpp_nng)
    add_executable(gtest_nng
        nng_endpoint_test.cpp
        nng_runtime_test.cpp
    )

    target_link_libraries(gtest_nng PRIVATE
        cxx_options
        zpp::zpp_nng
        GTest::gtest_main
    )

    target_include_directories(gtest_nng PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )

    gtest_discover_tests(gtest_nng
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DISCOVERY_TIMEOUT 30
    )

    add_run_target(gtest_nng)
endif()

if(ZPP_BUILD_FOLLY_MODULE)
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

    gtest_discover_tests(gtest_folly
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DISCOVERY_TIMEOUT 30
    )

    add_run_target(gtest_folly ARGS --folly_global_cpu_executor_threads=16 --folly_global_io_executor_threads=4)
endif()
