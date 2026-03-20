file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(zpp_core
    ${SVR_SOURCES}
)

target_link_libraries(zpp_core PRIVATE
    zpp::folly
    cxx_options
)

target_include_directories(zpp_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(zpp_core ARGS 
    ${PROJECT_SOURCE_DIR}/doc/server.json zpp_core)