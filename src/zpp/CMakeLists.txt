file(GLOB_RECURSE ZPP_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")

add_library(zpp STATIC
    ${ZPP_SOURCES}
)
add_library(zpp::zpp ALIAS zpp)

# 确保目标有合适的使用范围与导出名称
target_include_directories(zpp
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(zpp PUBLIC
    rapidjson::rapidjson
    spdlog::spdlog
    Threads::Threads
)

# 安装目标库（包含运行时/库/归档产物）
install(TARGETS zpp
    EXPORT zppTargets
    RUNTIME DESTINATION bin            # Windows 等上的 .dll
    LIBRARY DESTINATION lib            # Unix-like 上的 .so/.dylib
    ARCHIVE DESTINATION lib            # 静态库 .a/.lib（即使当前是 SHARED，也可保留通用性）
    INCLUDES DESTINATION include
)

# 安装头文件
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION include
)

# 导出 targets 文件，供 find_package(zpp CONFIG) 使用
install(EXPORT zppTargets
    NAMESPACE zpp::
    DESTINATION lib/cmake/zpp
    FILE zppTargets.cmake
)

include(CMakePackageConfigHelpers)

# 设定版本信息
set(zpp_VERSION ${PROJECT_VERSION})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfigVersion.cmake
    VERSION ${zpp_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Config 文件负责先恢复 zpp PUBLIC 依赖目标，再导入导出的 targets。
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/zppConfig.cmake
"include(CMakeFindDependencyMacro)\n"
"get_filename_component(_zpp_install_prefix \"\${CMAKE_CURRENT_LIST_DIR}\" PATH)\n"
"get_filename_component(_zpp_install_prefix \"\${_zpp_install_prefix}\" PATH)\n"
"get_filename_component(_zpp_install_prefix \"\${_zpp_install_prefix}\" PATH)\n"
"set(_zpp_deps_build_type \"\${CMAKE_BUILD_TYPE}\")\n"
"if(NOT _zpp_deps_build_type)\n"
"  set(_zpp_deps_build_type \"Release\")\n"
"endif()\n"
"set(_zpp_deps_dir \"\${_zpp_install_prefix}/lib/cmake/zeta_deps/\${_zpp_deps_build_type}\")\n"
"if(NOT EXISTS \"\${_zpp_deps_dir}\")\n"
"  message(FATAL_ERROR \"zpp requires zeta_forge deps package configs at: \${_zpp_deps_dir}. Run $ENV{ZETAX_ROOT}/zeta_forge/zbuild.py deps --BUILD_TYPE=\${_zpp_deps_build_type} --install first.\")\n"
"endif()\n"
"list(PREPEND CMAKE_PREFIX_PATH \"\${_zpp_deps_dir}\")\n"
"find_dependency(Threads)\n"
"find_dependency(RapidJSON CONFIG)\n"
"\n"
"if(NOT TARGET spdlog::spdlog)\n"
"  find_dependency(spdlog CONFIG)\n"
"  if(TARGET spdlog::spdlog_header_only AND NOT TARGET spdlog::spdlog)\n"
"    add_library(spdlog::spdlog INTERFACE IMPORTED)\n"
"    set_target_properties(spdlog::spdlog PROPERTIES INTERFACE_LINK_LIBRARIES spdlog::spdlog_header_only)\n"
"  endif()\n"
"  if(NOT TARGET spdlog::spdlog)\n"
"    message(FATAL_ERROR \"zpp requires spdlog::spdlog. Install zeta_forge deps first with: $ENV{ZETAX_ROOT}/zeta_forge/zbuild.py deps --install\")\n"
"  endif()\n"
"endif()\n"
"\n"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/zppTargets.cmake\")\n"
"unset(_zpp_deps_dir)\n"
"unset(_zpp_deps_build_type)\n"
"unset(_zpp_install_prefix)\n"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfigVersion.cmake
    DESTINATION lib/cmake/zpp
)
