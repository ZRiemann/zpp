#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <zpp/coro/task.h>
#include <zpp/namespace.h>

NSB_CORO

/**
 * @brief Simple single-threaded executor used to safely post work (resume
 * coroutines) onto a dedicated thread.
 *
 * This executor runs submitted tasks on a single background thread. It is a
 * tiny convenience used by the `worker_something` awaiter below to ensure
 * coroutine `resume()` is executed on a stable thread context rather than
 * directly from arbitrary detached threads.
 */
class simple_executor {
public:
  using work_t = std::function<void()>;

  static simple_executor &instance() {
    static simple_executor ex;
    return ex;
  }

  void post(work_t f) {
    {
      std::scoped_lock lk(m_);
      q_.push(std::move(f));
    }
    cv_.notify_one();
  }

  ~simple_executor() {
    stop_.store(true, std::memory_order_relaxed);
    cv_.notify_one();
    if (thr_.joinable())
      thr_.join();
  }

private:
  simple_executor() {
    thr_ = std::thread([this]() { run(); });
  }

  void run() {
    while (!stop_.load(std::memory_order_relaxed)) {
      work_t job;
      {
        std::unique_lock lk(m_);
        cv_.wait(lk, [&] {
          return stop_.load(std::memory_order_relaxed) || !q_.empty();
        });
        if (stop_.load(std::memory_order_relaxed) && q_.empty())
          return;
        job = std::move(q_.front());
        q_.pop();
      }
      if (job)
        job();
    }
  }

  std::thread thr_;
  std::mutex m_;
  std::condition_variable cv_;
  std::queue<work_t> q_;
  std::atomic<bool> stop_{false};
};

/**
 * @brief Safe awaiter that performs a delay and posts resume to a single
 * executor thread.
 *
 * Behavior:
 * - `await_ready()` returns true for non-positive durations (no suspend).
 * - `await_suspend(h)` spawns a short-lived sleeper thread that sleeps for
 *   the requested duration then posts a resume task to `simple_executor`.
 * - `await_resume()` is a no-op.
 *
 * This pattern keeps the actual `resume()` call on a controlled thread and
 * therefore avoids directly calling `resume()` from arbitrary threads.
 */
struct awaiter_delay {
  std::chrono::milliseconds dur;

  bool await_ready() const noexcept { return dur.count() <= 0; }

  void await_suspend(std::coroutine_handle<> h) const {
    // Spawn a sleeper thread; after sleeping, post resume to executor.
    std::thread([h, d = dur]() mutable {
      std::this_thread::sleep_for(d);
      simple_executor::instance().post([h]() mutable { h.resume(); });
    }).detach();
  }

  void await_resume() const noexcept {}
};

/**
 * @brief Awaiter adapter to make `task<PromiseType>` awaitable as a free
 * operator. This mirrors the previous `task::awaiter` implementation but is
 * provided here so awaiting behavior can be shared and kept separate from
 * the `task` class definition.
 */
template <typename ResultType, typename AwaiterInit = std::suspend_never,
          typename AwaiterFinal = std::suspend_always>
struct awaiter_task {
  std::coroutine_handle<z::coro::promise<ResultType, AwaiterInit, AwaiterFinal>>
      h;

  using result_type = typename z::coro::promise<ResultType, AwaiterInit,
                                                AwaiterFinal>::result_type;
  bool await_ready() const noexcept { return !h || h.done(); }

  bool await_suspend(std::coroutine_handle<> awaiting) {
    h.promise().set_continuation(awaiting);
    h.resume();
    return true;
  }

  decltype(auto) await_resume() {
    if constexpr (!std::is_void_v<result_type>) {
      auto v = h.promise().value_.value();
      if (h)
        h.destroy();
      return v;
    } else {
      if (h)
        h.destroy();
    }
  }
};

// Free-function operator co_await so `co_await task<...>` works without a
// member awaiter on `task`.
template <typename ResultType, typename AwaiterInit = std::suspend_never,
          typename AwaiterFinal = std::suspend_always>
awaiter_task<ResultType, AwaiterInit, AwaiterFinal>
operator co_await(z::coro::task<ResultType, AwaiterInit, AwaiterFinal> &t) {
  return awaiter_task<ResultType, AwaiterInit, AwaiterFinal>{t.handle};
}

template <typename ResultType, typename AwaiterInit = std::suspend_never,
          typename AwaiterFinal = std::suspend_always>
awaiter_task<ResultType, AwaiterInit, AwaiterFinal>
operator co_await(z::coro::task<ResultType, AwaiterInit, AwaiterFinal> &&t) {
  auto h = t.handle;
  t.handle = {};
  return awaiter_task<ResultType, AwaiterInit, AwaiterFinal>{h};
}

/**
 * @brief Example producer coroutine returning an `int`.
 *
 * This coroutine returns a `task` parameterized with the project's
 * `promise_result<int>` promise type. The `task` wrapper in
 * `zpp/coro/task.h` owns the coroutine handle and provides `result()` for
 * retrieving the stored value when available.
 *
 * @returns task<promise_result<int>> A move-only handle-wrapper for the
 * coroutine.
 *
 * Example usage:
 * @code
 * auto t = producer();
 * // With the default awaiters used in promise_result, the coroutine runs to
 * // completion immediately and the value can be retrieved via:
 * int v = t.result();
 * @endcode
 */
static inline task<int> producer() { co_return 123; }
/**
 * @brief Synchronous example that demonstrates creating and using `producer()`.
 *
 * This helper constructs the coroutine, then returns its stored result. It is
 * useful for tests and examples where the promise uses default awaiters that
 * run the coroutine to completion immediately.
 */
static inline int producer_run() {
  auto t = producer();
  return t.result();
}

NSE_CORO