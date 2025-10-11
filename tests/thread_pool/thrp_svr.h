#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>

//#define USE_CQUE
//#define USE_CQUE_AOP
//#define USE_CQUE_AOP_CLASS
//#define USE_TASK
//#define USE_THREAD_POOL_FIFO
#define BENCHMARK_ENGINE_FIFO

#if defined USE_CQUE
/****************************************************************************\
 * max_count[10000000] reached. elapsed 545 ms, avg 18,348,000 q/s 非阻塞版
 * max_count[10000000] reached. elapsed 1748 ms, avg 5,724,000 q/s 阻塞版
 * max_count[10000000] reached. elapsed 519 ms, avg 19,267,000 q/s 非阻塞+monitor
 * 他添加monitor_guard来统计任务以及线程情况，(基本)不影响吞吐量；
\****************************************************************************/
#include "scheduler_benchmark_cque.h"
#include <zpp/core/engine_cque/thread_pool.h>
#elif defined USE_CQUE_AOP
/****************************************************************************\
 * max_count[10000000] reached. elapsed 908 ms, avg 11,013,000 q/s 非阻塞版
 * max_count[10000000] reached. elapsed 2464 ms, avg 4,058,000 q/s 阻塞版
 * 使用 cque_aop 加入了回调和指针函数调用，调用比 cque 效率降低了 40%。
 * 虽然避免了继承虚函数表带来的性能损失，但设计复杂不容易扩展维护。
 * 考虑出一个aop继承多态实现版本，对比性能损失如何（结果类继承多太版效率略高，建议继承版）；
 * 阻塞版，非阻塞版 空闲时CPU使用1.3% 基本一致；
\****************************************************************************/
#include "scheduler_benchmark_cque_aop.h"
#include <zpp/core/engine_cque/thread_pool.h>
#elif defined USE_CQUE_AOP_CLASS
/****************************************************************************\
 * 非阻塞版本空闲到首个任务提取，延时不可控！
 * max_count[10000000] reached. elapsed 807 ms, avg 12,391,000 q/s 非阻塞版
 * max_count[10000000] reached. elapsed 2196 ms, avg 4,553,000 q/s 阻塞版
 * 测试吞吐量比 cque_aop 略高 10%，设计扩展也简单；
 * 推荐使用继承多态版本
 */
#include "scheduler_benchmark_cque_aop_class.h"
#include <zpp/core/engine_cque/thread_pool.h>
#elif defined USE_TASK
/******************************************************************************
 * 分装task,scheduler_aop类，实现benchmark功能
 * check fails 0 total tasks: 10,000,000 elapsed ms: 953 q/s: 10,493,000
 */
#include "benchmark_task.h"
#include <zpp/core/engine_cque/scheduler.h>
#include <zpp/core/engine_cque/thread_pool.h>
#elif defined USE_THREAD_POOL_FIFO
#include <zpp/core/engine_fifo/scheduler.h>
#include <zpp/core/engine_fifo/thread_pool.h>
#elif defined BENCHMARK_ENGINE_FIFO
#include "behchmark_engine_fifo.h"
#include <zpp/core/engine/scheduler.h>
#include <zpp/core/engine/thread_pool.h>
#endif


NSB_ZPP

class thrp_svr : public server{
public:
    thrp_svr(int argc, char** argv);
    ~thrp_svr() override;
    err_t configure() override;
    err_t run() override;
    err_t stop() override;
    err_t on_timer() override;
    err_t timer() override;
private:
#ifdef USE_CQUE
    scheduler_benchmark_cque _scheduler;
    thread_pool<scheduler_benchmark_cque> _thr_pool;
#elif defined USE_CQUE_AOP
    scheduler_benchmark_cque_aop _scheduler;
    thread_pool<scheduler_benchmark_cque_aop> _thr_pool;
#elif defined USE_CQUE_AOP_CLASS
    scheduler_benchmark_cque _scheduler;
    thread_pool<scheduler_benchmark_cque> _thr_pool;
#elif defined USE_TASK
    scheduler<task> _scheduler;
    thread_pool<scheduler<task>> _thr_pool;
    task_benchmark _benchmark{MAX_COUNT};
#elif defined USE_THREAD_POOL_FIFO
    scheduler<size_t> _scheduler;
    thread_pool<size_t> _thr_pool;
#elif defined BENCHMARK_ENGINE_FIFO
    scheduler<engine::aop> _scheduler;
    thread_pool<engine::aop> _thr_pool;
#endif
    int _thr_num{3};
    int _task_num{10};

};
NSE_ZPP