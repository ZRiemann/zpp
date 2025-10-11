cmake_minimum_required(VERSION 3.15)

project(zpp VERSION 0.1.0)

include(./cmake_util/common.cmake)

# Append User define options
#target_compile_options(cxx_options INTERFACE
#    $<${gcc_like_cxx}:$<BUILD_INTERFACE:-Wno-unused-result]>>
#)

# find packages
find_package(Threads REQUIRED)
find_package(folly REQUIRED)
message(STATUS "Found folly: ${FOLLY_INCLUDE_DIRS} ${FOLLY_LIBRARIES}")
include(./cmake_util/find_package/gtest.cmake)

#set(RAPIDJSON_ROOT "~/tmp" CACHE PATH "RapidJSON include root") # 指定用户目录，不使用CPM下载
include(./cmake_util/find_package/rapidjson.cmake)

#set(SPDLOG_ROOT "~/tmp" CACHE PATH "SPDLOG include root")
include(./cmake_util/find_package/spdlog.cmake)

include(./cmake_util/find_package/nng.cmake)

set(TASKFLOW_ROOT "~/git/taskflow" CACHE PATH "TASKFLOW_ROOT include root")
include(./cmake_util/find_package/taskflow.cmake)

#include(./cmake_util/find_package/CUDA.cmake)
#include(./cmake_util/find_package/grpc.cmake)
#include(./cmake_util/find_package/jmalloc.cmake)
#include(./cmake_util/find_package/modbus.cmake)
#include(./cmake_util/find_package/OpenCV.cmake)
#include(./cmake_util/find_package/protobuf.cmake)


message(STATUS "Begin building...")


if(WIN32)
    message(STATUS "System: Windows")
elseif(APPLE)
    message(STATUS "System: macOS")
elseif(UNIX)
    #set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    message(STATUS "System: Unix")
endif()

add_subdirectory(tests)
add_subdirectory(src)
add_subdirectory(examples)

