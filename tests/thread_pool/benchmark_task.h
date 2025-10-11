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

/**
 * @brief 任务基准测试数据
 */
class task_benchmark{
public:
    task_benchmark(size_t tasks)
        : _tasks(tasks)
        , _elapsed_ms(0)
        , _done(false){
        _hits = new int[_tasks];
        for(size_t i = 0; i < _tasks; ++i){
            _hits[i] = 0;
        }
    }
    ~task_benchmark(){
        delete [] _hits;
    }

    void tick(){
        spd_inf("test task begin...");
        _stop_watch.start();    
    }
    void tock(){
        _elapsed_ms = _stop_watch.elapsed_ms();
        spd_inf("test tasks done.");
        _done = true;
    }

    size_t check(){
        size_t fails{0};
        for(size_t i = 0; i < _tasks; ++i){
            if(_hits[i] != 1){
                ++fails;
            }
        }
        return fails;
    }

    void print(){ 
        if(_elapsed_ms == 0){
            _elapsed_ms = 1; // avoid divide by 0
        }
        size_t fails = check();
        spd_inf("\n\tcheck fails {}\n\ttotal tasks: {}\n\telapsed ms: {}\n\tq/s: {}\n", fails, CTS(_tasks), CTS(_elapsed_ms), CTS((_tasks / _elapsed_ms) * 1000));
    }

    bool is_done(){return _done;}
    size_t tasks(){return _tasks;}
private:
    size_t _tasks;
    time_t _elapsed_ms;
    bool _done;
    time _stop_watch;
    int *_hits;
    friend class task;
};

/**
 * @brief 任务吞吐量的基准测试(benchmark of task throughtput)
 */
class task : public aop{
public:
    task(task_benchmark &bm):_benchmark(bm){}
    ~task() = default;

    void operate(){
        switch(_index){
        case 0:
            _benchmark.tick();
            break;
        case MAX_COUNT - 1:
            _benchmark.tock();
            break;
        default:
            break;
        }
        if((size_t)_index < _benchmark._tasks){
            ++_benchmark._hits[_index];
        }

        if(MAX_COUNT < 10){
            // 功能测试时输出详细信息，压力测试时不输出信息
            spd_inf("task[{}] opreate...", _index);   
        }

        if(!free()){
            spd_err("failed to recycle task.");
        }
    }
public: // member veriable
    int _index{0};
    task_benchmark &_benchmark;
public: // task pool
    static obj_pool<task> pool;
    static void init_pool(size_t capacity, task_benchmark& bc){
        for(size_t i = 0; i < capacity; ++i){
            pool.push(new task(bc));
        }
        spd_inf("init pool capacity[{}]", capacity);
    }
    static void fini_pool(){
        task *t{nullptr};
        size_t count{0};
        while(pool.pop(t)){
            delete t;
            ++count;
        }
        spd_inf("fini task pool size[{}]", count);
    }
    static inline task* alloc(int i){
        task* t{nullptr};
        if(pool.pop(t)){
            t->_index = i;
        }else{
            // spd_war("task::pool is empty！")    
        }
        return t;
    }
    inline bool free(){
        return pool.push(this);
    }
};

NSE_ZPP