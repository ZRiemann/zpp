#pragma once
#include <zpp/namespace.h>
#include <folly/coro/Task.h>
#include <folly/coro/BlockingWait.h>
#include <folly/Try.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/coro/CurrentExecutor.h>
#include <utility>
#include <type_traits>

NSB_FOLLY

template <typename Awaitable>
inline auto co_cpu(Awaitable&& exec) {
    return folly::coro::co_withExecutor(
        folly::getGlobalCPUExecutor(), std::forward<Awaitable>(exec));
}

// Extract value type from folly::coro::Task<T>
template <typename T>
struct task_value {
    static constexpr bool valid = false;
};

template <typename R>
struct task_value<folly::coro::Task<R>> {
    using type = R;
    static constexpr bool valid = true;
};

// co_try: wrap an awaitable (folly::coro::Task<T>) into a Task<folly::Try<T>>
template <typename Awaitable>
inline auto co_try(Awaitable&& a)
    -> folly::coro::Task<folly::Try<typename task_value<std::remove_cv_t<std::remove_reference_t<Awaitable>>>::type>> {
    using AwaitT = std::remove_cv_t<std::remove_reference_t<Awaitable>>;
    static_assert(task_value<AwaitT>::valid, "co_try requires a folly::coro::Task<T>-like awaitable");
    auto r = co_await folly::coro::co_awaitTry(std::forward<Awaitable>(a));
    co_return r;
}

template <typename Awaitable>
inline auto co_io(Awaitable&& exec) {
    return folly::coro::co_withExecutor(
        folly::getGlobalIOExecutor(), std::forward<Awaitable>(exec));
}

// Accept either an awaitable or a callable returning an awaitable.
template <typename T>
inline auto co_blocking_wait(T&& t) {
    if constexpr (std::is_invocable_v<T>) {
        return folly::coro::blockingWait(std::forward<T>(t)());
    } else {
        return folly::coro::blockingWait(std::forward<T>(t));
    }
}
NSE_FOLLY
