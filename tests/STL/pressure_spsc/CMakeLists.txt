file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")

add_executable(pressure_spsc
    ${SVR_SOURCES}
)

target_link_libraries(pressure_spsc PRIVATE
    zpp
    Threads::Threads
    cxx_options
)

target_include_directories(pressure_spsc PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(pressure_spsc ARGS ${PROJECT_SOURCE_DIR}/doc/server.json server)