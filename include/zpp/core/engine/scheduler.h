#pragma once

#include <zpp/STL/mpmc.hpp>
#include <zpp/namespace.h>
#include <zpp/system/atomic_bit.h>

#define ENABLE_SCHEDULER_LOG 0
#if ENABLE_SCHEDULER_LOG
#include <zpp/spdlog.h>
#endif

#include "thread_pool.h"
#include <condition_variable>
#include <memory>
#include <vector>

NSB_ZPP
/**
 * @brief aop 调度器
 */
template <typename T, typename U = uint64_t> class scheduler {
  struct thr_ctx {
    int tid{-1};
    mpmc<T *> *que{nullptr};
    std::mutex mtx;
    std::condition_variable_any cv;
  };

public:
  scheduler(size_t thr_num, time_t timeout = 100000)
      : thr_num_(thr_num > sizeof(U) ? sizeof(U) : thr_num), timeout_(timeout) {
    index_.store(0);
    bool valid{true};
    size_t num{thr_num_};
    for (size_t i = 0; i < thr_num_; ++i) {
      ctxs_[i].que = new mpmc<T *>(8192, 8);
      if (!valid) {
        // memory insufficient
        delete ctxs_[i].que;
        ctxs_[i].que = nullptr;
        --num;
      }
    }
    thr_num_ = num;
  }

  ~scheduler() {
    for (size_t i = 0; i < thr_num_; ++i) {
      // need clear que tasks
      delete ctxs_[i].que;
    }
  }
  /**
   * @brief async operator producer
   * @note
   * 1. 优先分派给空闲线程；
   * 2.
   * 无空闲线程，采用公平队列模式，对应工作线程可能正在处理积压的任务，直接分发不需要消耗同步资源通知；
   * 3. 小概率极端情况，查找时无空闲，只派后干好空闲，通过等待超时触发调度；
   */
  bool dispatch(T *t) {
    if (!t) {
      return false;
    }
    static thread_local bool cursor_initialized = false;
    static thread_local size_t cursor = 0;
    if (!cursor_initialized) {
      cursor = index_.fetch_add(1, std::memory_order_relaxed) % thr_num_;
      cursor_initialized = true;
    }
    size_t index;
    if (idels_.try_pop_lsb(index)) {
      thr_ctx &ctx = ctxs_[index];
      std::unique_lock<std::mutex> lock(ctx.mtx);
      if (ctx.que->push(t)) {
        ctx.cv.notify_one();
#if ENABLE_SCHEDULER_LOG
        spd_inf("thr[{}] dispatch task[{}] to thread[{}]", tid::id(),
                fmt::ptr(t), ctx.tid);
#endif
        return true;
      }
#if ENABLE_SCHEDULER_LOG
      spd_war("thr[{}] dispatch task[{}] to thread[{}] failed", tid::id(),
              fmt::ptr(t), ctx.tid);
#endif
      idels_.clear_bit(static_cast<U>(1) << index);
      return false;
    }
#if ENABLE_SCHEDULER_LOG
    cursor = (cursor + 1) % thr_num_;
    index = cursor;
    thr_ctx &ctx = ctxs_[index];
    bool ret = ctx.que->push(t);
    spd_inf("thr[{}] dispatch task[{}] to thread[{}] ret[{}]", tid::id(),
            fmt::ptr(t), ctx.tid, ret);
    return ret;
#else
    cursor = (cursor + 1) % thr_num_;
    return ctxs_[cursor].que->push(t);
#endif
  }

public:
  size_t thr_num_;           // 线程池线程数量
  time_t timeout_;           // 线程等待超时 us
  std::atomic_size_t index_; // 线程idx
  thr_ctx ctxs_[sizeof(U)];
  atomic_bit<U> idels_; // 线程池状态
  friend class thread_pool<scheduler<T>>;
};
NSE_ZPP