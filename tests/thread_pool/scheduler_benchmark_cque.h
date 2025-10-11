#pragma once

#include "common.h"
#include <zpp/types.h>
#include <zpp/error.h>
#include <zpp/namespace.h>
#ifdef USE_BLOCK_QUE
#include <zpp/moodycamel/blockingconcurrentqueue.h>
#else
#include <zpp/moodycamel/concurrentqueue.h>
#endif
#include <zpp/system/time.h>
#include <zpp/spdlog.h>
#include <zpp/system/sleep.h>
#include <zpp/core/monitor.h>

NSB_ZPP
/**
 * @brief aop 调度器
 */
class scheduler_benchmark_cque{
public:
    scheduler_benchmark_cque() = default;
    ~scheduler_benchmark_cque() = default;
    /**
     * @brief async operator producer
     */
    err_t dispatch(int i){
        static thread_local moodycamel::ProducerToken ptoken(_queue);
        if(!_queue.enqueue(ptoken, i)){
            return ERR_FAIL;
        }
        return ERR_OK;
    }
    /**
     * @brief async opertor consumer
     */
    void schedule(){
        int i;
        thread_local moodycamel::ConsumerToken ctoken(_queue);
#ifdef USE_BLOCK_QUE
        if(!_queue.wait_dequeue_timed(i, 10)){
            return;
        }
#else
        if(!_queue.try_dequeue(ctoken, i)){
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            return;
        }
#endif
        do{
            monitor_guard mon_guard;
            switch(i){
            case 0:
                spd_inf("count begin...");
                _stopwatch.start();
            break;
            case MAX_COUNT - 1:
                spd_inf("max_count[{}] reached. elapsed {} ms, avg {} q/s", MAX_COUNT, _stopwatch.elapsed_ms(), CTS((MAX_COUNT / _stopwatch.elapsed_ms()) * 1000));
                _done = true;
            break;
            default:
            break;
            }
        }while(_queue.try_dequeue(ctoken, i));
    }
public:
    bool _done{false};
    time _stopwatch;
#ifdef USE_BLOCK_QUE
    moodycamel::BlockingConcurrentQueue<int> _queue;
#else
    moodycamel::ConcurrentQueue<int> _queue;
#endif
};
NSE_ZPP