file(GLOB_RECURSE SVR_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(zpp_hpx_basic
    ${SVR_SOURCES}
)

target_link_libraries(zpp_hpx_basic PRIVATE
    zpp::zpp
    HPX::hpx
    cxx_options
)

add_run_target(zpp_hpx_basic ARGS 
    ${PROJECT_SOURCE_DIR}/doc/server.json zpp_hpx_basic
    # 设定 HPX 线程数为 3 个（可根据需要调整）
    --hpx:threads=3
    # HPX 默认采用 Busy-Waiting（忙等待） 策略，80% CPU 占用率较高。
    # 让 HPX 变得“懒惰”一些，1.5% CPU 占用率，更友好地与其他库（如 Taskflow）协作。
    #--hpx:ini=hpx.max_idle_loop_count=0
)