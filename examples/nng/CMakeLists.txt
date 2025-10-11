#================================================================================
add_executable(nng_pub
    pub.cpp
)

target_link_libraries(nng_pub PRIVATE
    zpp::zpp_nng
    # zpp nng::nng Threads::Threads
    cxx_options
)

target_include_directories(nng_pub PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(nng_pub ARGS ${PROJECT_SOURCE_DIR}/doc/server.json nng_pub)

#================================================================================
add_executable(nng_sub
    sub.cpp
)

target_link_libraries(nng_sub PRIVATE
    zpp_nng zpp
    nng::nng
    Threads::Threads
    cxx_options
)

target_include_directories(nng_sub PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(nng_sub ARGS ${PROJECT_SOURCE_DIR}/doc/server.json nng_sub)

#================================================================================
add_executable(engine
    engine.cpp task_foo.cpp
)

target_link_libraries(engine PRIVATE
    zpp::zpp_nng
    cxx_options
)

target_include_directories(engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(engine ARGS ${PROJECT_SOURCE_DIR}/doc/server.json engine)
