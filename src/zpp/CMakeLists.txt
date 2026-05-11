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
"find_dependency(Threads)\n"
"\n"
"if(NOT TARGET rapidjson::rapidjson)\n"
"  find_package(RapidJSON QUIET CONFIG)\n"
"  if(TARGET rapidjson AND NOT TARGET rapidjson::rapidjson)\n"
"    add_library(rapidjson::rapidjson INTERFACE IMPORTED)\n"
"    set_target_properties(rapidjson::rapidjson PROPERTIES INTERFACE_LINK_LIBRARIES rapidjson)\n"
"  elseif(NOT TARGET rapidjson::rapidjson AND DEFINED RAPIDJSON_INCLUDE_DIRS AND NOT \"\${RAPIDJSON_INCLUDE_DIRS}\" STREQUAL \"\")\n"
"    add_library(rapidjson::rapidjson INTERFACE IMPORTED)\n"
"    set_target_properties(rapidjson::rapidjson PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"\${RAPIDJSON_INCLUDE_DIRS}\")\n"
"  elseif(DEFINED ENV{ZETAX_ROOT} AND EXISTS \"\$ENV{ZETAX_ROOT}/zeta_forge/cmake_util/find_package/rapidjson.cmake\")\n"
"    include(\"\$ENV{ZETAX_ROOT}/zeta_forge/cmake_util/find_package/rapidjson.cmake\")\n"
"  endif()\n"
"  if(NOT TARGET rapidjson::rapidjson)\n"
"    message(FATAL_ERROR \"zpp requires rapidjson::rapidjson\")\n"
"  endif()\n"
"endif()\n"
"\n"
"if(NOT TARGET spdlog::spdlog)\n"
"  find_dependency(spdlog CONFIG)\n"
"  if(TARGET spdlog::spdlog_header_only AND NOT TARGET spdlog::spdlog)\n"
"    add_library(spdlog::spdlog INTERFACE IMPORTED)\n"
"    set_target_properties(spdlog::spdlog PROPERTIES INTERFACE_LINK_LIBRARIES spdlog::spdlog_header_only)\n"
"  endif()\n"
"endif()\n"
"\n"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/zppTargets.cmake\")\n"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfigVersion.cmake
    DESTINATION lib/cmake/zpp
)
