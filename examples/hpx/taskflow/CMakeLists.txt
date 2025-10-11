file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(example_hpx_taskflow
    ${SVR_SOURCES}
)

target_link_libraries(example_hpx_taskflow PRIVATE
    zpp::zpp
    HPX::hpx
    Taskflow::Taskflow
    cxx_options
)

add_run_target(example_hpx_taskflow ARGS 
    ${PROJECT_SOURCE_DIR}/doc/server.json example_hpx_taskflow
    # 设定 HPX 线程数为 3 个（可根据需要调整）
    --hpx:threads=3
    # HPX 默认采用 Busy-Waiting（忙等待） 策略，80% CPU 占用率较高。
    # 让 HPX 变得“懒惰”一些，1.5% CPU 占用率，更友好地与其他库（如 Taskflow）协作。
    #--hpx:ini=hpx.max_idle_loop_count=0
    )