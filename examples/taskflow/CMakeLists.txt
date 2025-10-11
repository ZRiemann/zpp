file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(zpp_taskflow
    ${SVR_SOURCES}
)

target_link_libraries(zpp_taskflow PRIVATE
    zpp::zpp
    Taskflow::Taskflow
    cxx_options
)

add_run_target(zpp_taskflow ARGS 
    ${PROJECT_SOURCE_DIR}/doc/server.json zpp_taskflow)