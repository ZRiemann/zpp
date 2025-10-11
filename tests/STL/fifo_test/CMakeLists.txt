file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")

add_executable(test_stl
    ${SVR_SOURCES}
)

target_link_libraries(test_stl PRIVATE
    zpp
    Threads::Threads
    cxx_options
)

target_include_directories(test_stl PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(test_stl ARGS ${PROJECT_SOURCE_DIR}/doc/server.json server)
