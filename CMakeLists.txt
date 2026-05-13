cmake_minimum_required(VERSION 3.15)

project(zpp VERSION 0.5.1)

option(ZPP_USE_CONAN "Use Conan-provided dependencies and disable CPM fallbacks" OFF)
option(ZPP_BUILD_FOLLY_MODULE "Build zpp folly module" OFF)
option(ZPP_BUILD_NNG_MODULE "Build zpp nng module" OFF)
option(ZPP_BUILD_TASKFLOW_MODULE "Build zpp taskflow examples/module support" OFF)
option(ZPP_BUILD_TESTS "Build zpp tests" ON)
option(ZPP_BUILD_EXAMPLES "Build zpp examples" ON)
option(ZPP_BUILD_HPX_EXAMPLES "Build zpp HPX examples" ON)

set(ZETA_CMAKE_UTIL_DIR "" CACHE PATH "Path to zeta_forge/cmake_util")
if(NOT ZETA_CMAKE_UTIL_DIR AND DEFINED ENV{ZETAX_ROOT})
    set(_zeta_cmake_util_from_workspace "$ENV{ZETAX_ROOT}/zeta_forge/cmake_util")
    if(EXISTS "${_zeta_cmake_util_from_workspace}/common.cmake")
        set(ZETA_CMAKE_UTIL_DIR "${_zeta_cmake_util_from_workspace}" CACHE PATH "Path to zeta_forge/cmake_util" FORCE)
    endif()
endif()

if(NOT ZETA_CMAKE_UTIL_DIR OR NOT EXISTS "${ZETA_CMAKE_UTIL_DIR}/common.cmake")
    message(FATAL_ERROR
        "zeta_forge/cmake_util not found. Build zpp with ./zbuild.py, "
        "or set ZETAX_ROOT=/path/to/workspace so $ZETAX_ROOT/zeta_forge/cmake_util exists, "
        "or pass -DZETA_CMAKE_UTIL_DIR=/path/to/zeta_forge/cmake_util."
    )
endif()

include("${ZETA_CMAKE_UTIL_DIR}/common.cmake")

# Append User define options
#target_compile_options(cxx_options INTERFACE
#    $<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wno-unused-result]>>
#)

# find packages
find_package(Threads REQUIRED)
if(ZPP_BUILD_TESTS)
    include("${ZETA_CMAKE_UTIL_DIR}/find_package/gtest.cmake")
endif()

#set(RAPIDJSON_ROOT "~/tmp" CACHE PATH "RapidJSON include root") # 指定用户目录，不使用CPM下载
include("${ZETA_CMAKE_UTIL_DIR}/find_package/rapidjson.cmake")

#set(SPDLOG_ROOT "~/tmp" CACHE PATH "SPDLOG include root")
include("${ZETA_CMAKE_UTIL_DIR}/find_package/spdlog.cmake")

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
    add_subdirectory(tests)
endif()

if(ZPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
