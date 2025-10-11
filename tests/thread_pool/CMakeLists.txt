file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")

add_executable(thread_pool_benchmark
    ${SVR_SOURCES}
)

target_link_libraries(thread_pool_benchmark PRIVATE
    zpp
    Threads::Threads
    cxx_options
)

target_include_directories(thread_pool_benchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(thread_pool_benchmark ARGS ${PROJECT_SOURCE_DIR}/doc/server.json thread_pool_benchmark)