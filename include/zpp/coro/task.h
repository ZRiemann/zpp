#pragma once

#include <coroutine>
#include <type_traits>
#include <utility>
#include <optional>
#include <zpp/namespace.h>
#include <zpp/coro/promise.h>

NSB_CORO

/**
 * @file task.h
 * @brief Generic move-only coroutine task wrapper parameterized by result type.
 *
 * This header provides a small, generic `task<ResultType, AwaiterInit, AwaiterFinal>`
 * wrapper that holds the coroutine handle for a coroutine whose `promise_type`
 * is `z::coro::promise<ResultType, AwaiterInit, AwaiterFinal>` (exposed as
 * the nested `promise_type`). The wrapper is movable but not copyable. For
 * non-void result types the `result()` accessor returns the stored result
 * from the promise.
 */

/**
 * @brief Generic task wrapper parameterized by coroutine result type.
 *
 * @tparam ResultType The coroutine's result type (use `void` for no result).
 * @tparam AwaiterInit Initial awaiter behaviour for suspension at coroutine
 *                    entry (default: `std::suspend_never`).
 * @tparam AwaiterFinal Final awaiter behaviour for suspension at coroutine
 *                     exit (default: `std::suspend_always`).
 *
 * The wrapper does not itself provide `co_await` semantics; it only owns the
 * coroutine handle and exposes the nested `promise_type` to retrieve results
 * produced by the coroutine. Users may extend this wrapper or provide
 * custom awaiters if they require awaiting behaviour.
 */
template <
  typename ResultType = void,
  typename AwaiterInit = std::suspend_never,
  typename AwaiterFinal = std::suspend_always>
struct task {
  using promise_type = z::coro::promise<ResultType, AwaiterInit, AwaiterFinal>;
  using result_type = typename promise_type::result_type;

  std::coroutine_handle<promise_type> handle;

  task(std::coroutine_handle<promise_type> h) noexcept : handle(h) {}

  task(task&& o) noexcept : handle(o.handle) { o.handle = {}; }
  task& operator=(task&& o) noexcept {
    if (this != &o) {
      if (handle) handle.destroy();
      handle = o.handle;
      o.handle = {};
    }
    return *this;
  }

  task(const task&) = delete;
  task& operator=(const task&) = delete;

  ~task() {
    if (handle) handle.destroy();
  }

  /**
   * @brief Release ownership of the coroutine handle to the caller.
   *
   * After `release()` the `task` no longer owns or will destroy the frame.
   * The caller becomes responsible for calling `destroy()` on the returned
   * handle when appropriate.
   *
   * @return std::coroutine_handle<promise_type> The raw coroutine handle.
   */
  std::coroutine_handle<promise_type> release() noexcept {
    auto h = handle;
    handle = {};
    return h;
  }

  /**
   * @brief Detach ownership: drop the handle without destroying the frame.
   *
   * Use with caution: detaching will intentionally leak the coroutine frame
   * unless the returned raw handle was stored elsewhere. Prefer `release()`
   * when transferring ownership explicitly.
   */
  void detach() noexcept { handle = {}; }

  /**
   * @brief Whether the wrapper currently owns a coroutine handle.
   */
  bool valid() const noexcept { return static_cast<bool>(handle); }

  /**
   * @brief Destroy the owned coroutine (if any) and release the handle.
   */
  void destroy() noexcept {
    if (handle) {
      handle.destroy();
      handle = {};
    }
  }

  /**
   * @brief Retrieve the coroutine result for non-void result types.
   *
   * This function is only available when `promise_type::result_type` is not
   * `void`. It returns the value stored in the promise's internal storage.
   * Behavior is undefined if the coroutine never produced a value.
   */
  template <typename R = result_type>
  requires(!std::is_void_v<R>)
  R result() {
    return handle.promise()._value.value();
  }

  /**
   * @brief Dummy result accessor for `void` coroutines.
   */
  template <typename R = result_type>
  requires(std::is_void_v<R>)
  void result() noexcept {}
// (awaiter implementation moved to include/zpp/coro/awaiter.h)
};

NSE_CORO