cmake_minimum_required(VERSION 3.15)

set(ZPP_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
if(NOT EXISTS "${ZPP_VERSION_FILE}")
    message(FATAL_ERROR "zpp VERSION file not found: ${ZPP_VERSION_FILE}")
endif()

file(STRINGS "${ZPP_VERSION_FILE}" ZPP_VERSION_RAW LIMIT_COUNT 1)
string(STRIP "${ZPP_VERSION_RAW}" ZPP_VERSION_RAW)
if(NOT ZPP_VERSION_RAW MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR
        "Invalid zpp VERSION value: ${ZPP_VERSION_RAW}\n"
        "Expected format: vX.Y.Z, for example v0.5.2."
    )
endif()
set(ZPP_PROJECT_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

project(zpp VERSION ${ZPP_PROJECT_VERSION})

option(ZPP_BUILD_FOLLY_MODULE "Build zpp folly module" OFF)
option(ZPP_BUILD_NNG_MODULE "Build zpp nng module" OFF)
option(ZPP_BUILD_TASKFLOW_MODULE "Build zpp taskflow examples/module support" OFF)
option(ZPP_BUILD_HPX_MODULE "Build zpp HPX integration module" OFF)
option(ZPP_BUILD_TESTS "Build zpp tests" ON)
option(ZPP_BUILD_EXAMPLES "Build zpp examples" ON)
option(ZPP_BUILD_HPX_EXAMPLES "Build zpp HPX examples" OFF)

if(ZPP_BUILD_HPX_EXAMPLES)
    set(ZPP_BUILD_HPX_MODULE ON CACHE BOOL "Build zpp HPX integration module" FORCE)
endif()

set(ZETA_CMAKE_UTIL_DIR "" CACHE PATH "Path to zeta_forge/cmake_util")
if(NOT ZETA_CMAKE_UTIL_DIR OR NOT EXISTS "${ZETA_CMAKE_UTIL_DIR}/common.cmake")
    message(FATAL_ERROR
        "zeta_forge/cmake_util not found. Configure zeta_forge environment first, then build zpp with ./zbuild.py. "
        "For direct CMake usage, pass -DZETA_CMAKE_UTIL_DIR=/path/to/zeta_forge/cmake_util."
    )
endif()

include("${ZETA_CMAKE_UTIL_DIR}/common.cmake")

# Append User define options
#target_compile_options(cxx_options INTERFACE
#    $<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wno-unused-result]>>
#)

set(ZETA_DEPS_CMAKE_DIR "" CACHE PATH "Path to installed zeta_forge deps CMake package files")
if(NOT ZETA_DEPS_CMAKE_DIR OR NOT EXISTS "${ZETA_DEPS_CMAKE_DIR}")
    message(FATAL_ERROR
        "zeta_forge deps package configs not found. Configure/install zeta_forge deps first, then build zpp with ./zbuild.py. "
        "For direct CMake usage, pass -DZETA_DEPS_CMAKE_DIR=/path/to/lib/cmake/zeta_deps/<BUILD_TYPE>."
    )
endif()

list(PREPEND CMAKE_PREFIX_PATH "${ZETA_DEPS_CMAKE_DIR}")

# find packages
find_package(Threads REQUIRED)
if(ZPP_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
endif()

find_package(RapidJSON CONFIG REQUIRED)

find_package(spdlog CONFIG REQUIRED)
if(TARGET spdlog::spdlog_header_only AND NOT TARGET spdlog::spdlog)
    add_library(spdlog::spdlog INTERFACE IMPORTED)
    set_target_properties(spdlog::spdlog PROPERTIES
        INTERFACE_LINK_LIBRARIES spdlog::spdlog_header_only
    )
endif()

if(ZPP_BUILD_HPX_MODULE)
    if(NOT ZPP_HPX_DEPENDENCY_CMAKE_DIR OR
       NOT EXISTS "${ZPP_HPX_DEPENDENCY_CMAKE_DIR}/AsioConfig.cmake" OR
       NOT EXISTS "${ZPP_HPX_DEPENDENCY_CMAKE_DIR}/HwlocConfig.cmake")
        message(FATAL_ERROR
            "ZPP_HPX_DEPENDENCY_CMAKE_DIR must point to the zeta_forge HPX "
            "compatibility package directory. Build through ./zbuild.py or "
            "pass zeta_forge/builder/hpx/cmake/conan_compat/packages."
        )
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${ZPP_HPX_DEPENDENCY_CMAKE_DIR}")
    find_package(Asio CONFIG REQUIRED)
    find_package(Hwloc CONFIG REQUIRED)
    find_package(HPX CONFIG REQUIRED)
endif()
if(NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR
        "spdlog::spdlog not found. Install zeta_forge deps first with: "
        "$ENV{ZETAX_ROOT}/zeta_forge/zbuild.py deps --BUILD_TYPE=${CMAKE_BUILD_TYPE} --install"
    )
endif()

#include("${ZETA_CMAKE_UTIL_DIR}/find_package/CUDA.cmake")
#include("${ZETA_CMAKE_UTIL_DIR}/find_package/grpc.cmake")
#include("${ZETA_CMAKE_UTIL_DIR}/find_package/jmalloc.cmake")
#include("${ZETA_CMAKE_UTIL_DIR}/find_package/modbus.cmake")
#include("${ZETA_CMAKE_UTIL_DIR}/find_package/OpenCV.cmake")
#include("${ZETA_CMAKE_UTIL_DIR}/find_package/protobuf.cmake")


message(STATUS "Begin building...")


if(WIN32)
    message(STATUS "System: Windows")
elseif(APPLE)
    message(STATUS "System: macOS")
elseif(UNIX)
    #set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    message(STATUS "System: Unix")
endif()

add_subdirectory(src)
if(ZPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ZPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
