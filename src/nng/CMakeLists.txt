find_package(nng CONFIG REQUIRED)

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

include(CMakePackageConfigHelpers)

set(zpp_nng_VERSION ${PROJECT_VERSION})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/zpp_nngConfigVersion.cmake
    VERSION ${zpp_nng_VERSION}
    COMPATIBILITY SameMajorVersion
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/zpp_nngConfig.cmake
"include(CMakeFindDependencyMacro)\n"
"get_filename_component(_zpp_nng_install_prefix \"\${CMAKE_CURRENT_LIST_DIR}\" PATH)\n"
"get_filename_component(_zpp_nng_install_prefix \"\${_zpp_nng_install_prefix}\" PATH)\n"
"get_filename_component(_zpp_nng_install_prefix \"\${_zpp_nng_install_prefix}\" PATH)\n"
"set(_zpp_nng_deps_build_type \"\${CMAKE_BUILD_TYPE}\")\n"
"if(NOT _zpp_nng_deps_build_type)\n"
"  set(_zpp_nng_deps_build_type \"Release\")\n"
"endif()\n"
"set(_zpp_nng_deps_dir \"\${_zpp_nng_install_prefix}/lib/cmake/zeta_deps/\${_zpp_nng_deps_build_type}\")\n"
"if(EXISTS \"\${_zpp_nng_deps_dir}\")\n"
"  list(PREPEND CMAKE_PREFIX_PATH \"\${_zpp_nng_deps_dir}\")\n"
"endif()\n"
"find_dependency(zpp CONFIG)\n"
"find_dependency(nng CONFIG)\n"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/zpp_nngTargets.cmake\")\n"
"unset(_zpp_nng_deps_dir)\n"
"unset(_zpp_nng_deps_build_type)\n"
"unset(_zpp_nng_install_prefix)\n"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zpp_nngConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zpp_nngConfigVersion.cmake
    DESTINATION lib/cmake/zpp_nng
)
