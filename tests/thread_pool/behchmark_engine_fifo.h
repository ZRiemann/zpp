#pragma once

#include <zpp/namespace.h>
#include <zpp/core/engine/operator.h>
#include <zpp/spdlog.h>
#include <zpp/system/stopwatch/stopwatch_tsc.h>
#include <zpp/system/sleep.h>
#include <zpp/system/tid.h>
#include "common.h"

#define TEST_BENCHMARK_ENGINE_FIFO
//#define TEST_TASK_INFINITE
NSB_ZPP

#ifdef TEST_BENCHMARK_ENGINE_FIFO
/**
 * @brief 吞吐量基准测试
 */
class task_benchmark_engine_fifo : public engine::aop{
public:
    task_benchmark_engine_fifo(stopwatch_tsc &sw, size_t qid, size_t id, size_t end_task)
        :_qid(qid), _id(id), _end_task(end_task), _stopwatch(sw){

    }
    ~task_benchmark_engine_fifo() = default;

    void operate() override {
        if(_id == 0){
            spd_inf("banchmark[{}] begin...", _qid);
            _stopwatch.start();
        }else if(_id == _end_task){
            uint64_t ns = _stopwatch.elapsed_ns();
            spd_inf("que[{}] count[{}] time[{}][now:{}] done. Q/S:{}", _qid, CTS(_end_task + 1), CTS(ns), 
                CTS(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()), 
                CTS((_end_task + 1) * 1000000000 / ns));
        }//else{}
    }

public:
    size_t _qid;
    size_t _id;
    size_t _end_task;
    //std_chrono_stopwatch& _stopwatch;
    stopwatch_tsc& _stopwatch;
};
using tbef = task_benchmark_engine_fifo;
#elif defined TEST_TASK_INFINITE
/**
 * @brief 分发处理稳定性基准测试，不考虑内存分配释放问题。
 * @note 永远都不结束任务
 */
template<typename Scheduler>
class task_infinite : public aop{
public:
    task_infinite(Scheduler& scheduler, size_t id):_scheduler(scheduler), _id{id}, _count{0}{}
    ~task_infinite() = default;

    void operate() override{
        if(!(_count & 0x3ffff)){
            spd_inf("thr[{}] task[{}] repeats[{}]", tid::id(), fmt::ptr(this), _count);
        }
        ++_count;
        bool ret = _scheduler.dispatch(this);
        while(!ret){
            z::sleep(1);
            spd_war("thr[{}] task[{}] dispatch failed, try again...", tid::id(), fmt::ptr(this));
            ret = _scheduler.dispatch(this);
        }
    }
public:
    Scheduler& _scheduler;
    size_t _id;
    size_t _count;
};
#endif
NSE_ZPP