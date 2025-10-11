file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")

add_executable(uni_svr
    ${SVR_SOURCES}
)

target_link_libraries(uni_svr PRIVATE
    zpp
    Threads::Threads
    cxx_options
)

target_include_directories(uni_svr PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(uni_svr ARGS ${PROJECT_SOURCE_DIR}/doc/server.json uni_svr)