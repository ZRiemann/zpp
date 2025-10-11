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
        _scope.add(folly::coro::co_withExecutor(
            _keep_cpu, std::forward<Awaitable>(a)));
    }

    // Same as add_cpu but the task is tracked by the cancellable scope.
    template<typename Awaitable>
    inline void add_cpu_c(Awaitable&& a){
        _cancellable_scope.add(folly::coro::co_withExecutor(
            _keep_cpu, std::forward<Awaitable>(a)));
    }

    template<typename Awaitable>
    inline folly::coro::Task<void> co_cpu(Awaitable&& a){
        co_await folly::coro::co_withExecutor(
            _keep_cpu, std::forward<Awaitable>(a));
        co_return;
    }

    template<typename Awaitable>
    inline folly::coro::Task<void> co_io(Awaitable&& a){
        co_await folly::coro::co_withExecutor(
            _keep_io, std::forward<Awaitable>(a));
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
    folly::Init _folly_init;
    folly::coro::AsyncScope _scope;
    folly::coro::CancellableAsyncScope _cancellable_scope;

    folly::Executor::KeepAlive<> _keep_cpu = folly::getGlobalCPUExecutor();
    folly::Executor::KeepAlive<> _keep_io  = folly::getGlobalIOExecutor();
};
NSE_FOLLY