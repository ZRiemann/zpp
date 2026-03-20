file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(folly_server
    ${SVR_SOURCES}
)

target_link_libraries(folly_server PRIVATE
    zpp::folly
    cxx_options
)

target_include_directories(folly_server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(folly_server ARGS ${PROJECT_SOURCE_DIR}/doc/server.json folly_server --folly_global_cpu_executor_threads=8 --folly_global_io_executor_threads=4)