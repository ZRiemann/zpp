add_executable(inspector_svr
	inspector_svr.cpp
)
target_link_libraries(inspector_svr PRIVATE
	zpp::folly
	zpp::zpp
)

add_executable(inspector_cli
	inspector_cli.cpp
)
target_link_libraries(inspector_cli PRIVATE
	zpp::folly
	zpp::zpp
)

add_run_target(inspector_cli ARGS ${PROJECT_SOURCE_DIR}/doc/server.json folly_examples)

add_run_target(inspector_svr ARGS ${PROJECT_SOURCE_DIR}/doc/server.json folly_examples)