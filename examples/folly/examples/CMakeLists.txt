file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(folly_examples
    ${SVR_SOURCES}
)

target_link_libraries(folly_examples PRIVATE
    zpp::zpp
    Folly::folly
    #Boost::boost
    Threads::Threads
    cxx_options
)

target_include_directories(folly_examples PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${FOLLY_INCLUDE_DIRS}
    #${Boost_INCLUDE_DIRS}
)

add_run_target(folly_examples ARGS ${PROJECT_SOURCE_DIR}/doc/server.json folly_examples)