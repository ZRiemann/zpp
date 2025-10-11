#pragma once

#include <zpp/namespace.h>
#include <zpp/STL/fifo.h>
#include <zpp/system/atomic_bit.h>

#define ENABLE_SCHEDULER_LOG 0
#if ENABLE_SCHEDULER_LOG
#include <zpp/spdlog.h>
#endif

#include "thread_pool.h"
#include <vector>
#include <memory>
#include <condition_variable>

NSB_ZPP
/**
 * @brief aop 调度器
 */
template<typename T, typename U = uint64_t>
class scheduler{
    struct thr_ctx{
        int tid{-1};
        mpsc<T*>* que{nullptr};
        std::mutex mtx;
        std::condition_variable_any cv;
    };
public:
    scheduler(size_t thr_num, time_t timeout = 100000)
        :_thr_num(thr_num > sizeof(U) ? sizeof(U) : thr_num)
        ,_timeout(timeout){
        _index.store(0);
        bool valid;
        size_t num{_thr_num};
        for(size_t i = 0; i < _thr_num; ++i){
            _ctxs[i].que = new mpsc<T*>(valid, 8192, 8);
            if(!valid){
                // memory insufficient
                delete _ctxs[i].que;
                _ctxs[i].que = nullptr;
                --num;
            }
        }
        _thr_num = num;
    }

    ~scheduler(){
        for(size_t i = 0; i < _thr_num; ++i){
            // need clear que tasks
            delete _ctxs[i].que;
        }
    }
    /**
     * @brief async operator producer
     * @note
     * 1. 优先分派给空闲线程；
     * 2. 无空闲线程，采用公平队列模式，对应工作线程可能正在处理积压的任务，直接分发不需要消耗同步资源通知；
     * 3. 小概率极端情况，查找时无空闲，只派后干好空闲，通过等待超时触发调度；
     */
    bool dispatch(T* t){
        if(!t){
            return false;
        }
        static thread_local size_t index_ = _index++ % _thr_num;
        size_t index;
        if(_idels.try_pop_lsb(index)){
            thr_ctx& ctx = _ctxs[index];
            std::unique_lock<std::mutex> lock(ctx.mtx);
            if(ctx.que->push(t)){
                ctx.cv.notify_one();
#if ENABLE_SCHEDULER_LOG
                spd_inf("thr[{}] dispatch task[{}] to thread[{}]", tid::id(), fmt::ptr(t), ctx.tid);
#endif
                return true;
            }
#if ENABLE_SCHEDULER_LOG
                spd_war("thr[{}] dispatch task[{}] to thread[{}] failed", tid::id(), fmt::ptr(t), ctx.tid);
#endif
            _idels.clear_bit(static_cast<U>(1) << index);
            return false;
        }
#if ENABLE_SCHEDULER_LOG
        index = ++index_ % _thr_num;
        thr_ctx &ctx = _ctxs[index];
        bool ret = ctx.que->push(t);
        spd_inf("thr[{}] dispatch task[{}] to thread[{}] ret[{}]", tid::id(), fmt::ptr(t), ctx.tid, ret);
        return ret;
#else
        return _ctxs[++index_ % _thr_num].que->push(t);
#endif
    }
public:
    size_t _thr_num; // 线程池线程数量
    time_t _timeout; // 线程等待超时 us
    std::atomic_size_t _index; // 线程idx
    thr_ctx _ctxs[sizeof(U)];
    atomic_bit<U> _idels; // 线程池状态
    friend class thread_pool<scheduler<T>>;
};
NSE_ZPP