#include <folly/coro/AsyncScope.h>
#include <folly/coro/BlockingWait.h>
#include <folly/coro/WithCancellation.h>
#include <folly/executors/GlobalExecutor.h>
#include <memory>
#include <zpp/core/monitor.h>
#include <zpp/error.h>
#include <zpp/folly/server.h>
#include <zpp/spdlog.h>
#include <zpp/system/sleep.h>

USE_ZPP

fo::server::server(int argc, char **argv)
    : z::server(argc, argv), folly_init_(&argc, &argv) {
  ::folly::getGlobalCPUExecutor().add(std::move([](auto /*keepAlive*/) {
    monitor_guard mon_guard;
    spd_inf("global CPU executor task running...");
  }));
}

fo::server::~server() {
  // request cancellation and join scopes (RAII members)
  cancellable_scope_.requestCancellation();
  folly::coro::blockingWait(cancellable_scope_.cancelAndJoinAsync());
  folly::coro::blockingWait(scope_.joinAsync());
}

err_t fo::server::on_timer() {
#if 1
  auto counters = folly::getGlobalCPUExecutorCounters();
  spd_inf("GlobalCPUExecutor - threads: {}, active: {}, pending: {}",
          counters.numThreads, counters.numActiveThreads,
          counters.numPendingTasks);
#endif
  return ERR_OK;
}

err_t fo::server::timer() {
  sleep(3000);
  return ERR_OK;
}