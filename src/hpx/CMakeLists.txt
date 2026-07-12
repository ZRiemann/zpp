if(NOT ZPP_HPX_EXPECTED_GIT_COMMIT)
    message(FATAL_ERROR
        "ZPP_HPX_EXPECTED_GIT_COMMIT is required. Build zpp through ./zbuild.py "
        "or pass the commit recorded by zeta_forge/3rd/hpx."
    )
endif()

if(NOT HPX_GIT_COMMIT STREQUAL ZPP_HPX_EXPECTED_GIT_COMMIT)
    message(FATAL_ERROR
        "zpp HPX source mismatch: expected ${ZPP_HPX_EXPECTED_GIT_COMMIT}, "
        "but HPXConfig.cmake reports ${HPX_GIT_COMMIT}. Rebuild and install "
        "zeta_forge/3rd/hpx before configuring zpp."
    )
endif()

if(NOT HPX_WITH_FETCH_STDEXEC)
    message(FATAL_ERROR
        "zpp::hpx requires the zeta_forge HPX build configured with "
        "HPX_WITH_FETCH_STDEXEC=ON."
    )
endif()

add_library(zpp_hpx INTERFACE)
add_library(zpp::hpx ALIAS zpp_hpx)
set_target_properties(zpp_hpx PROPERTIES EXPORT_NAME hpx)

target_include_directories(zpp_hpx INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(zpp_hpx INTERFACE cxx_std_20)
target_link_libraries(zpp_hpx INTERFACE
    zpp::zpp
    HPX::hpx
)

install(TARGETS zpp_hpx
    EXPORT zpp_hpxTargets
    INCLUDES DESTINATION include
)

install(EXPORT zpp_hpxTargets
    NAMESPACE zpp::
    DESTINATION lib/cmake/zpp_hpx
    FILE zpp_hpxTargets.cmake
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/zpp_hpxConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/zpp_hpxConfig.cmake
"include(CMakeFindDependencyMacro)\n"
"set(_zpp_hpx_dependency_cmake_dir \"${ZPP_HPX_DEPENDENCY_CMAKE_DIR}\")\n"
"if(NOT EXISTS \"\${_zpp_hpx_dependency_cmake_dir}/AsioConfig.cmake\" OR NOT EXISTS \"\${_zpp_hpx_dependency_cmake_dir}/HwlocConfig.cmake\")\n"
"  message(FATAL_ERROR \"zpp_hpx dependency package directory is missing or incomplete: \${_zpp_hpx_dependency_cmake_dir}\")\n"
"endif()\n"
"list(PREPEND CMAKE_PREFIX_PATH \"\${_zpp_hpx_dependency_cmake_dir}\")\n"
"find_dependency(zpp CONFIG)\n"
"find_dependency(Asio CONFIG)\n"
"find_dependency(Hwloc CONFIG)\n"
"find_dependency(HPX CONFIG)\n"
"set(_zpp_hpx_expected_git_commit \"${ZPP_HPX_EXPECTED_GIT_COMMIT}\")\n"
"if(NOT HPX_GIT_COMMIT STREQUAL _zpp_hpx_expected_git_commit)\n"
"  message(FATAL_ERROR \"zpp_hpx requires HPX commit ${ZPP_HPX_EXPECTED_GIT_COMMIT}, but the selected HPX package reports \${HPX_GIT_COMMIT}.\")\n"
"endif()\n"
"if(NOT HPX_WITH_FETCH_STDEXEC)\n"
"  message(FATAL_ERROR \"zpp_hpx requires HPX_WITH_FETCH_STDEXEC=ON.\")\n"
"endif()\n"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/zpp_hpxTargets.cmake\")\n"
"unset(_zpp_hpx_dependency_cmake_dir)\n"
"unset(_zpp_hpx_expected_git_commit)\n"
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/zpp_hpxConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/zpp_hpxConfigVersion.cmake
    DESTINATION lib/cmake/zpp_hpx
)
