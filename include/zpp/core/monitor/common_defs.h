#pragma once

#include <zpp/namespace.h>
#include <zpp/types.h>

NSB_ZPP

typedef struct thread_state{
    uint64_t state; // 0-idel, 1-busy
    uint64_t tasks; // alltasks handled by this thread
    tsc_t start; // timestamp counter start value (relaxed reads)
    tsc_t task_cycles; // 统计每个线程总任务耗时 (timestamp counter)
    tsc_t idel_cycles; // 统计每个线程总空闲时间 (timestamp counter)
}thr_stat_t;

/**
 * @brief print thr_stat_t array
 */
void print_thr_stat(const thr_stat_t* states, size_t num_threads);

NSE_ZPP
