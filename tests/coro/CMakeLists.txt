file(GLOB_RECURSE CORO_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(gtest_coro
    ${CORO_TEST_SOURCES}
)

target_link_libraries(gtest_coro PRIVATE
    cxx_options
    GTest::gtest_main
    zpp::folly
)

target_include_directories(gtest_coro PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

include(GoogleTest)

gtest_discover_tests(gtest_coro
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DISCOVERY_TIMEOUT 30
)

add_run_target(gtest_coro)
