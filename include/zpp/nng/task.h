#pragma once

#include "aio.h"
#include "defs.h"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <vector>
#include <zpp/STL/mpmc.hpp>
#include <zpp/core/monitor.h>
#include <zpp/types.h>

NSB_NNG

template <typename Worker> class task_pool;

/**
 * @class task
 * @brief nng engine task template
 *
 * @details This adapter treats the NNG task queue as the process-wide business
 * execution engine.  Dispatching a task completes an internal AIO and lets NNG
 * run the worker on its task threads, so socket callbacks, timers, and internal
 * business continuations can share one runtime and one shutdown model.
 *
 * This design is intentionally different from adding a separate business thread
 * pool.  A separate pool can keep NNG I/O callbacks moving for longer, but it
 * also moves overload into another queue where latency, stale work, memory
 * pressure, and cancellation become harder to reason about.  Using only NNG
 * task threads makes saturation visible at the service boundary: when business
 * work is full, AIO completion and receive reposting slow down, which provides
 * natural back pressure to upstream producers.
 *
 * The trade-off is that NNG task threads become a shared critical resource.
 * Worker implementations should be short, bounded, and non-blocking with
 * respect to the same NNG task queue.  Long CPU work, external synchronous I/O,
 * waiting for another task in the same runtime, or unbounded locking can delay
 * AIO completions, timers, control-plane callbacks, and shutdown.  Production
 * users should pair this adapter with explicit capacity limits, overload policy
 * such as drop/reject/coalesce/timeout, in-flight draining before pool release,
 * and metrics for task latency, pool misses, dispatch failures, and callback
 * delay.
 *
 * @code
 * class worker{
 * public:
 *      err_t work(int state) noexcept{
 *          // working in an NNG task thread
 *          return ERR_OK;
 *      }
 * };
 *
 * void main(){
 *      task_pool<worker> pool(64);
 *      task<worker>* t{nullptr};
 *
 *      if(pool.acquire(t)){
 *          t.dispatch();
 *      }
 *      // do something else...
 * }
 * @endcode
 */
template <typename Worker> class task {
  friend class task_pool<Worker>;

public:
  /// Worker type stored inline by this task.
  using worker_type = Worker;

  /// Creates a task bound to its owning pool.
  explicit task(task_pool<Worker> &pool)
      : aio_(&task<Worker>::aio_handle_cb, this), pool_(pool) {}

  /**
   * @brief dispatch the task to nng task queue, and execute in nng task threads
   * @warning if `aio_` not complete dispatch cause fail. Maybe have some design
   * BUGs to FIX
   */
  inline bool dispatch() noexcept {
    if (pool_.stopped()) {
      (void)worker_.work(NNG_ESTOPPED);
      release_to_pool(NNG_ESTOPPED);
      return false;
    }
    bool is_ok = aio_.start(task<Worker>::aio_cancel_cb, this);
    if (is_ok) {
      aio_.finish(NNG_OK);
    } else {
      (void)worker_.work(NNG_ESTOPPED);
      release_to_pool(NNG_ESTOPPED);
    }
    return is_ok;
  }

  /// Returns the inline worker object.
  inline Worker &worker() noexcept { return worker_; }

  /// Returns the inline worker object.
  inline const Worker &worker() const noexcept { return worker_; }

  /// Returns the last NNG AIO result for this task.
  inline nng_err result() const noexcept { return aio_.result(); }

private:
  inline void handle() noexcept {
    nng_err err = aio_.result();
    (void)worker_.work(static_cast<int>(err));
    release_to_pool(err);
  }

  static void aio_handle_cb(void *h) {
    monitor_guard guard;
    ((task<Worker> *)h)->handle();
  }

  static void aio_cancel_cb(nng_aio *aio, void *user, nng_err err) noexcept {
    (void)user;
    nng_aio_finish(aio, err);
  }

private:
  inline void release_to_pool(nng_err err) noexcept {
    if (err == NNG_ESTOPPED) {
      return;
    }
    (void)pool_.release(this);
  }

  aio aio_;
  task_pool<Worker> &pool_;
  Worker worker_;
};

/**
 * @class task_pool
 * @brief Fixed-capacity task pool backed by an MPMC free queue.
 *
 * @details The pool owns all task objects and recycles them after their NNG AIO
 * callbacks complete.  Workers stay business-focused and do not need a
 * `release(task*)` method.  The pool should be created after the NNG runtime is
 * initialized and destroyed after callers stop dispatching tasks.  `stop()` is
 * a lifecycle barrier; producers should stop calling `acquire()` before the
 * pool is stopped or destroyed.
 */
template <typename Worker> class task_pool {
public:
  /// Task type owned by this pool.
  using task_type = task<Worker>;

  /// Preallocates a fixed number of reusable tasks.
  explicit task_pool(std::size_t capacity)
      : capacity_(capacity > 0 ? capacity : 1), free_(capacity_ + 1) {
    tasks_.reserve(capacity_);
    for (std::size_t index = 0; index < capacity_; ++index) {
      tasks_.push_back(std::make_unique<task_type>(*this));
      if (!free_.push(tasks_.back().get())) {
        throw std::runtime_error("failed to initialize nng task pool");
      }
    }
  }

  /// Stops all task AIO handles before releasing storage.
  ~task_pool() noexcept { stop(); }

  task_pool(const task_pool &) = delete;
  task_pool &operator=(const task_pool &) = delete;

  /// Acquires one reusable task from the pool.
  inline bool acquire(task_type *&task) noexcept { return free_.pop(task); }

  /// Returns one task to the pool after its callback completes or dispatch
  /// fails.
  inline bool release(task_type *task) noexcept { return free_.push(task); }

  /// Stops every task AIO and rejects future dispatches.
  inline void stop() noexcept {
    const bool was_stopped = stopped_.exchange(true, std::memory_order_acq_rel);
    if (was_stopped) {
      return;
    }
    for (auto &task : tasks_) {
      task->aio_.stop();
    }
  }

  /// Returns the number of tasks owned by this pool.
  inline std::size_t capacity() const noexcept { return capacity_; }

  /// Returns true after this pool has started rejecting future dispatches.
  inline bool stopped() const noexcept {
    return stopped_.load(std::memory_order_acquire);
  }

private:
  std::size_t capacity_{0};
  std::vector<std::unique_ptr<task_type>> tasks_;
  z::mpmc<task_type *> free_;
  std::atomic_bool stopped_{false};
};

NSE_NNG
