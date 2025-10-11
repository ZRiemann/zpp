#pragma once

#include <cstring>
#include <string>
#include <vector>

#include <zpp/namespace.h>
#include <zpp/system/tid.h>
#include <zpp/system/tsc.h>
#include <zpp/system/time.h>
#include <zpp/core/monitor/common_defs.h>

NSB_ZPP

#define ENABLE_MONITOR_GUARD 1
constexpr int MAX_THRS = 64;

typedef struct monitor_s{
    time begin;
    thread_state states[MAX_THRS];

    monitor_s() {
        begin.update();
        for (int i = 0; i < MAX_THRS; ++i) {
            states[i].start = tsc_now_r();
            states[i].state = 0;
            states[i].tasks = 0;
            states[i].task_cycles = 0;
            states[i].idel_cycles = 0;
        }
    }

    /**
     * @brief 任务开始时标记
     *
     * 使用 TSC relaxed 读取（低开销），并在统计空闲时间时用
     * `z::step_ns_r` 计算并更新线程的 `start` 值以避免序列化开销。
     */
    inline tsc_t tick() noexcept{
        thread_state& state = states[tid::id()];
        state.state  = 1;
        if(state.tasks){
            // 不统计启动无任务的空闲时段，从第一个任务开始统计空闲时段
            state.idel_cycles += step_cycles_r(state.start);
        }else{
            state.start = tsc_now_r();
        }
        return state.start;
    }

    /**
     * @brief 累积每个线程运行的任务数，注意控制线程数
     */
    inline tsc_t tock() noexcept{
        thread_state& state = states[tid::id()];
        state.tasks++;
        state.state = 0;
        state.task_cycles += step_cycles_r(state.start);
        return state.start;
    }
}mon_t;

struct monitor_guard{
    static mon_t mont;
    static void print_statistic();

    monitor_guard(){
#if ENABLE_MONITOR_GUARD
        mont.tick();
#endif
    }

    ~monitor_guard(){
#if ENABLE_MONITOR_GUARD
        mont.tock();
#endif
    }
};

NSE_ZPP