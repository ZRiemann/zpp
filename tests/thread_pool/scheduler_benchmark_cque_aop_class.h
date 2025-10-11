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
#include <zpp/core/async_operator.h>
#include <zpp/core/obj_pool.h>

NSB_ZPP

class task : public aop{
public:
    task(int i):_index(i){}
    ~task() = default;

    void operate(){
        if(MAX_COUNT < 10){
            // 功能测试时输出详细信息，压力测试时不输出信息
            spd_inf("task[{}] opreate...", _index);   
        }
    }
public:
    int _index;
public:
    static obj_pool<task> pool;
    static void init_pool(){
        for(int i = 0; i < MAX_COUNT; ++i){
            pool.push(new task(i));
        }
    }
    static inline task* pop(){
        task* t{nullptr};
        if(pool.pop(t)){
            // spd_war("task::pool is empty！")    
        }
        return t;
    }
    inline bool push(){
        return pool.push(this);
    }
};
/**
 * @brief aop 调度器
 */
class scheduler_benchmark_cque{
public:
    scheduler_benchmark_cque(){
        _hits = new int [MAX_COUNT];
        for(int i = 0; i < MAX_COUNT; ++i){
            _hits[i] = 0;
        }
    }
    ~scheduler_benchmark_cque() = default;
    /**
     * @brief async operator producer
     */
    err_t dispatch(aop* t){
        static thread_local moodycamel::ProducerToken ptoken(_queue);
        if(!_queue.enqueue(ptoken, t)){
            return ERR_FAIL;
        }
        return ERR_OK;
    }
    /**
     * @brief async opertor consumer
     */
    void schedule(){
        aop* a;
        thread_local moodycamel::ConsumerToken ctoken(_queue);
#ifdef USE_BLOCK_QUE
        if(!_queue.wait_dequeue_timed(a, 1000)){
            return;
        }
#else
        if(!_queue.try_dequeue(ctoken, a)){
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            return;
        }
#endif
        do{
            monitor_guard mon_guard;
            a->operate();
            task* t = dynamic_cast<task*>(a);
            if(!t){
                spd_err("not a task");
                break;
            }
            if(t->_index > _max_index){
                _max_index = t->_index;
            }
            ++_hits[t->_index];

            switch(t->_index){
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
            //t->push(); // 回收
        }while(_queue.try_dequeue(ctoken, a));
    }

    void check_hits(){
        int check_fails{0};
        // 检查是否每个index都调用一次
        for(int i = 0; i < MAX_COUNT; ++i){
            if(_hits[i] != 1){
                spd_err("hits[{}] : {}", i, _hits[i]);
                ++check_fails;
            }
        }
        spd_inf("check fails: {}", check_fails);
    }
public:
    bool _done{false};
    time _stopwatch;
    int _max_index{0}; // 排查 t->push() 导致的无法达到退出问题, _done = true 不可达！
    int *_hits; // 检查所有 index 都被调用一次
#ifdef USE_BLOCK_QUE
    moodycamel::BlockingConcurrentQueue<aop*> _queue;
#else
    moodycamel::ConcurrentQueue<aop*> _queue;
#endif
};
NSE_ZPP