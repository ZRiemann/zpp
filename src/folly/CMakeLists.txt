file(GLOB_RECURSE ZPP_FOLLY_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_library(zpp_folly STATIC
	${ZPP_FOLLY_SOURCES}
)
add_library(zpp::folly ALIAS zpp_folly)

target_include_directories(zpp_folly
	PRIVATE
		${PROJECT_SOURCE_DIR}/include
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(zpp_folly PUBLIC
	zpp::zpp
	Boost::boost
	Folly::folly
)

install(TARGETS zpp_folly
	EXPORT zpp_follyTargets
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
	INCLUDES DESTINATION include
)

install(EXPORT zpp_follyTargets
	NAMESPACE zpp::
	DESTINATION lib/cmake/zpp_folly
	FILE zpp_follyTargets.cmake
)

