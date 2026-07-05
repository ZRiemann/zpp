#================================================================================
add_executable(publisher_example
    publisher_example.cpp
)

target_link_libraries(publisher_example PRIVATE
    zpp::zpp_nng
    # zpp nng::nng Threads::Threads
    cxx_options
)

target_include_directories(publisher_example PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(publisher_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json publisher_example)

#================================================================================
add_executable(subscriber_example
    subscriber_example.cpp
)

target_link_libraries(subscriber_example PRIVATE
    zpp::zpp_nng
    cxx_options
)

target_include_directories(subscriber_example PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(subscriber_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json subscriber_example)

#================================================================================
add_executable(pub_sub_device_example
    pub_sub_device_example.cpp
)

target_link_libraries(pub_sub_device_example PRIVATE
    zpp::zpp_nng
    cxx_options
)

target_include_directories(pub_sub_device_example PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_run_target(pub_sub_device_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json pub_sub_device_example)

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

#================================================================================
function(add_nng_protocol_example target)
    add_executable(${target}
        ${target}.cpp
    )

    target_link_libraries(${target} PRIVATE
        zpp::zpp_nng
        cxx_options
    )

    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
endfunction()

add_nng_protocol_example(pusher_example)
add_run_target(pusher_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json pusher_example)
add_nng_protocol_example(puller_example)
add_run_target(puller_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json puller_example)
add_nng_protocol_example(requester_example)
add_run_target(requester_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json requester_example)
add_nng_protocol_example(replier_example)
add_run_target(replier_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json replier_example)
add_nng_protocol_example(pair1_listen_example)
add_run_target(pair1_listen_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json pair1_listen_example)
add_nng_protocol_example(pair1_dial_example)
add_run_target(pair1_dial_example ARGS ${PROJECT_SOURCE_DIR}/doc/server.json pair1_dial_example)
