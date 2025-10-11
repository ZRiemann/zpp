file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(example_hpx_basic
    ${SVR_SOURCES}
)

target_link_libraries(example_hpx_basic PRIVATE
    #zpp::zpp
    HPX::hpx
    HPX::iostreams_component
    cxx_options
)

add_run_target(example_hpx_basic ARGS 
    ${PROJECT_SOURCE_DIR}/doc/server.json example_hpx_basic)