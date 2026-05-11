find_package(HPX REQUIRED)

add_subdirectory(basic)
if(ZPP_BUILD_TASKFLOW_MODULE)
    add_subdirectory(taskflow)
endif()
add_subdirectory(zpp)
