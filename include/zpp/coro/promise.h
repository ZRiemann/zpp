#pragma once

#include <coroutine>
#include <type_traits>
#include <utility>
#include <optional>
#include <variant>
#include <zpp/namespace.h>

NSB_CORO

/**
 * @file promise.h
 * @brief Lightweight coroutine promise templates used by small coroutine examples.
 *
 * These templates provide a minimal `promise_type` implementation that can be
 * used for both `void` and non-`void` coroutine return types. The implementation
 * is intentionally simple and configurable via the `AwaiterInit` and
 * `AwaiterFinal` template parameters so callers can choose `std::suspend_never` or
 * `std::suspend_always` (or custom awaiter types) for initial/final suspension.
 */

/**
 * @brief Primary (non-void) `promise` template.
 *
 * @tparam Result The coroutine result type. May be a reference, value or cv-qualified
 *                type; storage normalizes to a non-reference, non-cv type.
 * @tparam AwaiterInit The awaiter type returned from `initial_suspend()`.
 * @tparam AwaiterFinal The awaiter type returned from `final_suspend()`.
 *
 * The primary template stores the result in a `std::optional` of the
 * decayed `Result` type and exposes the standard coroutine promise interface:
 * - `get_return_object()` returns a `std::coroutine_handle<promise>` for the
 *   created promise object.
 * - `initial_suspend()` and `final_suspend()` return the provided awaiter
 *   types so callers can control suspension behavior.
 * - `return_value(...)` overloads accept lvalues and rvalues and emplace the
 *   value into the internal storage.
 */
template <
    typename Result,
    typename AwaiterInit,
    typename AwaiterFinal>
struct promise {
  using result_type = Result;
  using promise_type = promise<Result, AwaiterInit, AwaiterFinal>;

  std::optional<std::remove_cv_t<std::remove_reference_t<Result>>> _value{};
  /**
   * @brief Optional continuation handle used when another coroutine `co_await`s
   * this coroutine's `task` wrapper. When set, the final-suspend awaiter
   * will resume the continuation.
   */
  std::coroutine_handle<> _continuation{};

  /**
   * @brief Get the coroutine handle associated with this promise.
   *
   * @return std::coroutine_handle<promise_type> handle referencing the
   *         coroutine's promise object.
   */
  auto get_return_object() {
    return std::coroutine_handle<promise_type>::from_promise(*this);
  }

  /**
   * @brief Awaiter returned at coroutine entry.
   *
   * The returned type is `AwaiterInit` (default `std::suspend_never`). Use a
   * custom awaiter to control whether the coroutine suspends immediately.
   */
  AwaiterInit initial_suspend() noexcept { return {}; }

  /**
   * @brief Awaiter returned when the coroutine reaches final suspension.
   *
   * The returned type is `AwaiterFinal` (default `std::suspend_never`). See
   * C++ coroutine semantics for the meaning of final suspension.
   */
  /**
   * @brief Final-suspend awaiter that composes the user-provided
   * `AwaiterFinal` and resumes a stored continuation (if any).
   */
  struct final_awaiter {
    AwaiterFinal inner;
    promise* self;

    bool await_ready() noexcept { return inner.await_ready(); }

    template <typename H>
    decltype(auto) await_suspend(H h) noexcept(noexcept(inner.await_suspend(h))) {
      if constexpr (std::is_void_v<decltype(inner.await_suspend(h))>) {
        inner.await_suspend(h);
        if (self->_continuation) self->_continuation.resume();
      } else {
        auto r = inner.await_suspend(h);
        if (self->_continuation) self->_continuation.resume();
        return r;
      }
    }

    decltype(auto) await_resume() noexcept(noexcept(inner.await_resume())) {
      if constexpr (requires { inner.await_resume(); }) {
        return inner.await_resume();
      }
    }
  };

  final_awaiter final_suspend() noexcept { return final_awaiter{AwaiterFinal{}, this}; }

  /**
   * @brief Store a continuation handle to be resumed at final suspension.
   */
  void set_continuation(std::coroutine_handle<> c) noexcept { _continuation = c; }

  /**
   * @brief Accept an lvalue result from `co_return`.
   *
   * This overload participates in overload resolution only when `Result` is
   * copy-constructible. The implementation emplaces a copy of `v` into the
   * internal `std::optional` storage.
   *
   * @param v The lvalue to be copied into the coroutine result storage.
   */
  void return_value(const Result& v) noexcept(std::is_nothrow_copy_constructible_v<Result>) 
    requires std::is_copy_constructible_v<Result> {
    _value.emplace(v);
  }

  /**
   * @brief Accept an rvalue result from `co_return`.
   *
   * This overload is selected for prvalues and `std::move`d values. It is
   * constrained to be available only when `Result` is move-constructible and
   * will emplace the moved value into storage.
   *
   * Example:
   * @code
   * co_return Result{...};
   * co_return std::move(x);
   * @endcode
   *
   * @param v The rvalue to be moved into the coroutine result storage.
   */
  void return_value(Result&& v) noexcept(std::is_nothrow_move_constructible_v<Result>) 
    requires std::is_move_constructible_v<Result> {
    _value.emplace(std::move(v));
  }

  /**
   * @brief Called when an exception escapes the coroutine body.
   *
   * The minimal behavior is to terminate the process. Projects can override
   * this to capture or propagate exceptions as needed.
   */
  void unhandled_exception() { std::terminate(); }
};

// specialization for void
/**
 * @brief Specialization of `promise` for `void` return type.
 *
 * @tparam AwaiterInit Awaiter type for `initial_suspend()`.
 * @tparam AwaiterFinal Awaiter type for `final_suspend()`.
 *
 * The `void` specialization does not store a result value; it implements the
 * minimal coroutine promise interface required for `co_return;` or
 * `co_return;`-equivalent behavior.
 */
template <typename AwaiterInit, typename AwaiterFinal>
struct promise<void, AwaiterInit, AwaiterFinal> {
  using result_type = void;
  using promise_type = promise<void, AwaiterInit, AwaiterFinal>;

  /**
   * @brief Placeholder member to keep layout simple; not used for results.
   */
  std::monostate _value{}; // placeholder

  /**
   * @brief Continuation handle for awaiting coroutines.
   */
  std::coroutine_handle<> _continuation{};

  /**
   * @brief Get the coroutine handle for this promise.
   *
   * @return std::coroutine_handle<promise_type>
   */
  auto get_return_object() {
    return std::coroutine_handle<promise_type>::from_promise(*this);
  }

  /**
   * @brief Awaiter for entry.
   */
  AwaiterInit initial_suspend() noexcept { return {}; }

  /**
   * @brief Awaiter for final suspension.
   */
  struct final_awaiter {
    AwaiterFinal inner;
    promise* self;

    bool await_ready() noexcept { return inner.await_ready(); }

    template <typename H>
    decltype(auto) await_suspend(H h) noexcept(noexcept(inner.await_suspend(h))) {
      if constexpr (std::is_void_v<decltype(inner.await_suspend(h))>) {
        inner.await_suspend(h);
        if (self->_continuation) self->_continuation.resume();
      } else {
        auto r = inner.await_suspend(h);
        if (self->_continuation) self->_continuation.resume();
        return r;
      }
    }

    decltype(auto) await_resume() noexcept(noexcept(inner.await_resume())) {
      if constexpr (requires { inner.await_resume(); }) {
        return inner.await_resume();
      }
    }
  };

  final_awaiter final_suspend() noexcept { return final_awaiter{AwaiterFinal{}, this}; }

  void set_continuation(std::coroutine_handle<> c) noexcept { _continuation = c; }

  /**
   * @brief Called for `co_return;` in a `void` coroutine.
   */
  void return_void() noexcept {}

  /**
   * @brief Minimal unhandled exception handler.
   */
  void unhandled_exception() { std::terminate(); }
};

/**
 * @brief Convenience alias for a `void` promise with default awaiters.
 *
 * Example: `promise_void<>` is equivalent to
 * `promise<void, std::suspend_never, std::suspend_never>`.
 */
template <
  typename AwaiterInit = std::suspend_never,
  typename AwaiterFinal = std::suspend_always>
using promise_void = promise<void, AwaiterInit, AwaiterFinal>;

/**
 * @brief Convenience alias for a value-returning promise with default awaiters.
 *
 * @tparam Result The coroutine result type (default `int`).
 */
template <
  typename Result = int,
  typename AwaiterInit = std::suspend_never,
  typename AwaiterFinal = std::suspend_always>
using promise_result = promise<Result, AwaiterInit, AwaiterFinal>;

NSE_CORO