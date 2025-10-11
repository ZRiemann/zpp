#include "thrp_svr.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/system/sleep.h>
#include <zpp/core/monitor.h>
#include <zpp/json.h>
#include <zpp/system/stopwatch/stopwatch_tsc.h>
#include "common.h"

USE_ZPP

#if defined USE_CQUE_AOP_CLASS
obj_pool<task> task::pool;
#endif

#if defined USE_CQUE_AOP
obj_pool<aop> aop::pool;
#endif

#if defined USE_TASK
obj_pool<task> task::pool;
#endif

thrp_svr::thrp_svr(int argc, char** argv)
    :server(argc, argv)
#ifdef USE_THREAD_POOL_FIFO
    ,_scheduler(THR_NUM, 100)
#elif defined BENCHMARK_ENGINE_FIFO
    , _scheduler(THR_NUM, 100)
#endif
    {
}
err_t thrp_svr::configure(){
#if defined BENCHMARK_ENGINE_FIFO
    json conf;
    if(ERR_OK != conf.load_file(_argv[1])){
        exit(-1);
    }else{
        json svr_conf(conf);
        if(ERR_OK == conf.get_member(_argv[2], svr_conf)){
            svr_conf.get_int("thread_num", _thr_num);
            svr_conf.get_int("task_num", _task_num);
        }
    }
#elif defined USE_CQUE_AOP_CLASS
    task::init_pool();
#endif
#if defined USE_CQUE_AOP
    aop::init_pool();
#endif
#if defined USE_TASK
    task::init_pool(MAX_COUNT / 2, _benchmark);
#endif
    return ERR_OK;
}

err_t thrp_svr::run(){
#if defined BENCHMARK_ENGINE_FIFO
    _thr_pool.run(_scheduler);
#elif defined USE_THREAD_POOL_FIFO
    _thr_pool.run(_scheduler);
#else
    _thr_pool.run(_scheduler, 16);
#endif
    z::sleep_ms(100);

#if defined BENCHMARK_ENGINE_FIFO
#ifdef TEST_BENCHMARK_ENGINE_FIFO
    std::vector<std::jthread> p_thrs;
    for(size_t i = 0; i < _scheduler._thr_num; ++i){
        p_thrs.emplace_back(std::jthread([this, i](std::stop_token token){
            std::vector<tbef*> tasks;
            // 可能多线程操作stopwatch, 导致计时结果不准确。避免这种使用方式；所有类型的lock都有这问题。
            // time[258,797,830,318,818][epoc:18,446,744,073,709,551,360 now:1,764,209,058,480,855,440] done. Q/S:38 
            stopwatch_tsc stopwatch;
            for(size_t j = 0; j < static_cast<size_t>(_task_num); ++j){
                tasks.push_back(new tbef(stopwatch, i, j, _task_num - 1));
            }
            for(auto t : tasks){
                //spd_inf("push task<qid:{} id:{}>", t->_qid, t->_id);
                while(!_scheduler.dispatch(t)){
                    z::sleep_ms(1);
                }
            }
            spd_inf("push qid[{}] done.", i);
        }));
    }
#elif defined TEST_TASK_INFINITE
    //for(size_t i = 0; i < 1; ++i){ // 单任务无限循环
    for(size_t i = 0; i < _scheduler._thr_num * 10; ++i){ // 多任务无限循环
        auto t = new task_infinite(_scheduler, i);
        _scheduler.dispatch(t);
    }
#endif
#elif defined USE_THREAD_POOL_FIFO
    // 启动对等的过个线程推送
    //_scheduler.dispatch(nullptr); // 单线程推送有瓶颈
    /*
    18 22:22:12.628522 2173591 I thr_pool[2] notifys[1] timeouts[1] tasks[100,000,000] Q/S[17,164,000]      thread_pool.h:53
    18 22:22:12.731442 2173590 I thr_pool[1] notifys[2] timeouts[1] tasks[100,000,000] Q/S[16,866,000]      thread_pool.h:53
    18 22:22:13.555043 2173586 I on_timer...        thrp_svr.cpp:140
    18 22:22:13.555046 2173586 I timer...   thrp_svr.cpp:150
    18 22:22:13.558174 2173589 I thr_pool[0] notifys[3] timeouts[1] tasks[100,000,000] Q/S[14,801,000]      thread_pool.h:53
    18 22:22:13.628678 2173591 I thr_pool[2] notifys[1] timeouts[2] tasks[100,000,000] Q/S[17,161,000]      thread_pool.h:53
    */
    std::vector<std::jthread> p_thrs;
    for(size_t i = 0; i < _scheduler._thr_num; ++i){
        p_thrs.emplace_back(std::jthread([this, i](std::stop_token token){
            auto &ctx = this->_scheduler._ctxs[i];
            auto &que = *this->_scheduler._ctxs[i].que;
            spd_inf("producer que:{} {}", fmt::ptr(this->_scheduler._ctxs[i].que), fmt::ptr(&que));
            size_t *k{nullptr};
            size_t push_fails{0};
            for(size_t j = 0; j < MAX_COUNT; ++j){
                while(!que.push(k)){
#if 0
                    spd_inf("thr[{}] push [{}] failed", i, j);
                    spd_inf("front_chk:{}->{} back_chk:{}->{}", fmt::ptr(que._front_chunk), fmt::ptr(que._front_chunk->next), fmt::ptr(que._back_chunk), fmt::ptr(que._back_chunk->next));
#if FIFO_BACK_PTR_MODE == 1
                    spd_inf("front_ptr:{} back_ptr:{}", fmt::ptr(que._front_ptr), fmt::ptr(que._atm_back_ptr.load(std::memory_order_relaxed)));
#else
                    spd_inf("front_ptr:{} back_ptr:{}", fmt::ptr(que._front_ptr), fmt::ptr(que._back_ptr));
#endif
                    spd_inf("front_end_ptr:{} back_end_ptr:{}", fmt::ptr(que._front_end_ptr), fmt::ptr(que._back_end_ptr));
                    spd_inf("chunk_num:{} chunk_capacity:{} apare:{}", que._chunk_num.load(), que._chunk_capacity, fmt::ptr(que._spare_chunk.load()));
                    spd_inf("push_count:{} pop_count:{}", que._count_push, que._count_pop);
#endif
                    ++push_fails;
                    z::sleep(100);
                }
                if(j == 0){
                    //spd_inf("notify thr[{}]", i);
                    std::unique_lock<std::mutex> lock(ctx.mtx);
                    ctx.cv.notify_one();
                }
                if(!(j & 0x3fffff)){
                    spd_inf("thr_p[{}] push({})", i, j);
            }
            }
            spd_inf("p_thr[{}] push {} tasks done.", i, MAX_COUNT);
        }));
    }
#else
    int i{0};
#if defined USE_BLOCK_QUE
    spd_inf("using BlockingConcurrentQueue<T> BLOCK");
#else
    spd_inf("using ConcurrentQueue<T> NON-BLOCK");
#endif

    /* 为避免new/delete带来的开销，使用obj_pool预分配内存
     * 减少测量队列的吞吐量的外部干扰，实际情况需要考虑对象回收
     */
    for(; i < MAX_COUNT; ++i){
#if defined USE_CQUE_AOP
        //spd_inf("testing aop_t template mode...");
        aop* aop_p;
        if(aop::pool.pop(aop_p)){
            _scheduler.dispatch(aop_p->make_aop());
        }
#elif defined USE_CQUE_AOP_CLASS
        //spd_inf("testing aop class mode...");
        task* t = task::pop();
        if(t){
            t->_index = i; // 必须在这里设置 _index，cque不能保证复用顺序
            _scheduler.dispatch(t);
        }
#elif defined USE_TASK
        task* t = task::alloc(i);
        if(t){
            if(!_scheduler.dispatch(t)){
                t->free();
                spd_war("dispatch task[{}] failed", i);
            }
        }else{
            // task pool empty try again
            --i;
        }
#else
        //spd_inf("testing int enqueue/dequeue throughtput...");
        while(ERR_OK != _scheduler.dispatch(i)){
            spd_war("dispatch [{}] failed", i);
            z::sleep(1000);
        }
#endif
    }
    spd_inf("dispatch [{}] done.", i);
#endif
    return ERR_OK;    
}


err_t thrp_svr::stop(){
    spd_inf("server stop...");
#if defined BENCHMARK_ENGINE_FIFO
    _thr_pool.stop();
#elif defined USE_THREAD_POOL_FIFO
    _thr_pool.stop();
#elif defined USE_TASK
    _thr_pool.stop();
    spd_inf("thread pool post stop token.");
#else
    _scheduler.check_hits();
    spd_inf("stop thread pool... scheduler.max_index[{}]", _scheduler._max_index);
    _thr_pool.stop();
#endif
    return ERR_OK;
}

err_t thrp_svr::on_timer(){
#if defined BENCHMARK_ENGINE_FIFO
    monitor_guard::print_statistic();
    return ERR_END;
#elif  defined USE_THREAD_POOL_FIFO
    //spd_inf("on_timer...");
    return ERR_OK;
#elif defined USE_TASK
    return _benchmark.is_done() ? ERR_END : ERR_OK;
#else
    return _scheduler._done ? ERR_END : ERR_OK;
#endif
}

err_t thrp_svr::timer(){
    //spd_inf("timer...");
    sleep(10000);
    return ERR_OK;
}

thrp_svr::~thrp_svr(){
#ifdef USE_TASK
    spd_inf("print benchmark... {}", _benchmark.tasks());
    _benchmark.print();
    spd_inf("fini thread pool...");
    task::fini_pool();
#endif
    monitor_guard::print_statistic();
}
#define SVR_NAME thrp_svr
#include <zpp/core/main.hpp>