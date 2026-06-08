#pragma once

#include <folly/init/Init.h>
#include <memory>
#include <zpp/core/server.h>
#include <folly/coro/AsyncScope.h>
#include <folly/coro/WithCancellation.h>
#include <folly/coro/Task.h>
#include <folly/executors/GlobalExecutor.h>

NSB_FOLLY

class server : public z::server {
public:
    server(int argc, char** argv);
    ~server() override;
    err_t on_timer() override;
    err_t timer() override;
public:
    // Accept an awaitable (e.g. folly::coro::Task<void>) and schedule it
    // on the CPU executor associated with this server.
    template<typename Awaitable>
    inline void add_cpu(Awaitable&& a){
        scope_.add(folly::coro::co_withExecutor(
            keep_cpu_, std::forward<Awaitable>(a)));
    }

    // Same as add_cpu but the task is tracked by the cancellable scope.
    template<typename Awaitable>
    inline void add_cpu_c(Awaitable&& a){
        cancellable_scope_.add(folly::coro::co_withExecutor(
            keep_cpu_, std::forward<Awaitable>(a)));
    }

    template<typename Awaitable>
    inline folly::coro::Task<void> co_cpu(Awaitable&& a){
        co_await folly::coro::co_withExecutor(
            keep_cpu_, std::forward<Awaitable>(a));
        co_return;
    }

    template<typename Awaitable>
    inline folly::coro::Task<void> co_io(Awaitable&& a){
        co_await folly::coro::co_withExecutor(
            keep_io_, std::forward<Awaitable>(a));
        co_return;
    }

    // Accept KeepAlive by-value — it's cheap to copy/move and ensures
    // the executor token stays alive for the duration of the coroutine.
    template<typename Awaitable>
    inline folly::coro::Task<void> co_exe(Awaitable&& a, folly::Executor::KeepAlive<> keep){
        co_await folly::coro::co_withExecutor(
            keep, std::forward<Awaitable>(a));
        co_return;
    }
private:
    folly::Init folly_init_;
    folly::coro::AsyncScope scope_;
    folly::coro::CancellableAsyncScope cancellable_scope_;

    folly::Executor::KeepAlive<> keep_cpu_ = folly::getGlobalCPUExecutor();
    folly::Executor::KeepAlive<> keep_io_  = folly::getGlobalIOExecutor();
};
NSE_FOLLY