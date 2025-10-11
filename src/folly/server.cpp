#include <zpp/folly/server.h>
#include <folly/coro/AsyncScope.h>
#include <folly/coro/WithCancellation.h>
#include <memory>
#include <folly/coro/BlockingWait.h>
#include <zpp/spdlog.h>
#include <zpp/error.h>
#include <zpp/system/sleep.h>
#include <folly/executors/GlobalExecutor.h>
#include <zpp/core/monitor.h>

USE_ZPP

fo::server::server(int argc, char** argv)
    :z::server(argc, argv)
    ,_folly_init(&argc, &argv) {
    ::folly::getGlobalCPUExecutor().add(std::move([](auto /*keepAlive*/){
        monitor_guard mon_guard;
        spd_inf("global CPU executor task running...");
    }));
}

fo::server::~server(){
    // request cancellation and join scopes (RAII members)
    _cancellable_scope.requestCancellation();
    folly::coro::blockingWait(_cancellable_scope.cancelAndJoinAsync());
    folly::coro::blockingWait(_scope.joinAsync());
}

err_t fo::server::on_timer(){
#if 1
    auto counters = folly::getGlobalCPUExecutorCounters();
    spd_inf("GlobalCPUExecutor - threads: {}, active: {}, pending: {}",
        counters.numThreads,
        counters.numActiveThreads,
        counters.numPendingTasks);
#endif
      return ERR_OK;
}

err_t fo::server::timer(){
    sleep(3000);
    return ERR_OK;
}