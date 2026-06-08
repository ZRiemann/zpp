find_package(Taskflow QUIET CONFIG)

if(NOT TARGET Taskflow::Taskflow)
    if(DEFINED TASKFLOW_ROOT)
        message(STATUS "Using Taskflow from TASKFLOW_ROOT: ${TASKFLOW_ROOT}")
        add_library(Taskflow::Taskflow INTERFACE IMPORTED)
        set_target_properties(Taskflow::Taskflow PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${TASKFLOW_ROOT}"
        )
    else()
        message(FATAL_ERROR
            "Taskflow::Taskflow not found. Install zeta_forge managed Taskflow package configs "
            "or pass -DTASKFLOW_ROOT=/path/to/taskflow."
        )
    endif()
endif()

file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(zpp_taskflow
    ${SVR_SOURCES}
)

target_link_libraries(zpp_taskflow PRIVATE
    zpp::zpp
    Taskflow::Taskflow
    cxx_options
)

add_run_target(zpp_taskflow ARGS 
    ${PROJECT_SOURCE_DIR}/doc/server.json zpp_taskflow)
