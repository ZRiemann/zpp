add_executable(zpp_hpx_exec_pipeline pipeline.cpp)
target_link_libraries(zpp_hpx_exec_pipeline PRIVATE
    zpp::hpx
    cxx_options
)

add_executable(zpp_hpx_exec_scheduler scheduler.cpp)
target_link_libraries(zpp_hpx_exec_scheduler PRIVATE
    zpp::hpx
    cxx_options
)

add_run_target(zpp_hpx_exec_pipeline)
add_run_target(zpp_hpx_exec_scheduler)
