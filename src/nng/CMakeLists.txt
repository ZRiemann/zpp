file(GLOB_RECURSE ZPP_NNG_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")
add_library(zpp_nng STATIC
    ${ZPP_NNG_SOURCES}
)
add_library(zpp::zpp_nng ALIAS zpp_nng)

target_include_directories(zpp_nng
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(zpp_nng PUBLIC
    zpp::zpp
    nng::nng
)

# Install the library and register it in the export set so the
# subsequent `install(EXPORT ...)` has a known export name.
install(TARGETS zpp_nng
    EXPORT zpp_nngTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# 导出 targets 文件，供 find_package(zpp CONFIG) 使用
install(EXPORT zpp_nngTargets
    NAMESPACE zpp::
    DESTINATION lib/cmake/zpp_nng
    FILE zpp_nngTargets.cmake
)