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
set(zpp_VERSION 1.0.0)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfigVersion.cmake
    VERSION ${zpp_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 最简单的 Config 文件（可手写或更复杂地生成）
# 这里的 zppConfig.cmake 主要作用是导入导出的 targets
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/zppConfig.cmake
"include(\"\${CMAKE_CURRENT_LIST_DIR}/zppTargets.cmake\")\n"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zppConfigVersion.cmake
    DESTINATION lib/cmake/zpp
)
