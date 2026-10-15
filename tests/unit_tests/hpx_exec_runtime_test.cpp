#include <zpp/hpx/exec/exec.hpp>

#include <gtest/gtest.h>

#include <hpx/async.hpp>
#include <hpx/future.hpp>
#include <hpx/init.hpp>
#include <hpx/thread.hpp>

#include <exception>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ex = z::hpx::exec;

namespace {
int test_result = 1;
}

TEST(HpxExecRuntime, SyncWaitCompletesThreadPoolSender) {
  ASSERT_NE(nullptr, hpx::threads::get_self_ptr());

  const hpx::thread::id waiting_thread = hpx::this_thread::get_id();
  ex::thread_pool_scheduler scheduler;
  auto result = ex::sync_wait(ex::schedule(scheduler) | ex::then([] {
                                return hpx::this_thread::get_id();
                              }));

  ASSERT_TRUE(result);
  EXPECT_NE(waiting_thread, std::get<0>(*result));
}

TEST(HpxExecRuntime, WaitsForPendingFutureWithScheduler) {
  ex::thread_pool_scheduler scheduler;
  hpx::promise<std::unique_ptr<int>> promise;
  auto future = promise.get_future();
  auto producer = hpx::async([promise = std::move(promise)]() mutable {
    hpx::this_thread::yield();
    promise.set_value(std::make_unique<int>(42));
  });

  auto result = ex::sync_wait(
      ex::from_future(std::move(future), scheduler) |
      ex::continues_on(scheduler) |
      ex::then([](std::unique_ptr<int> value) { return *value; }));

  ASSERT_TRUE(result);
  EXPECT_EQ(42, std::get<0>(*result));
  producer.get();
}

TEST(HpxExecRuntime, PropagatesPendingFutureExceptionWithScheduler) {
  ex::thread_pool_scheduler scheduler;
  hpx::promise<int> promise;
  auto future = promise.get_future();
  auto producer = hpx::async([promise = std::move(promise)]() mutable {
    hpx::this_thread::yield();
    promise.set_exception(
        std::make_exception_ptr(std::runtime_error("delayed failure")));
  });

  EXPECT_THROW(static_cast<void>(ex::sync_wait(
                   ex::from_future(std::move(future), scheduler))),
               std::runtime_error);
  producer.get();
}

TEST(HpxExecRuntime, WaitsForPendingVoidFutureWithScheduler) {
  ex::thread_pool_scheduler scheduler;
  hpx::promise<void> promise;
  auto future = promise.get_future();
  auto producer = hpx::async([promise = std::move(promise)]() mutable {
    hpx::this_thread::yield();
    promise.set_value();
  });

  auto result = ex::sync_wait(ex::from_future(std::move(future), scheduler));

  ASSERT_TRUE(result);
  producer.get();
}

TEST(HpxExecRuntime, ConvertsScheduledSenderToFuture) {
  ex::thread_pool_scheduler scheduler;
  auto sender = ex::schedule(scheduler) |
                ex::then([] { return std::make_unique<int>(42); });
  static_assert(!ex::future_compatible_sender<decltype(sender)>);
  auto future = ex::into_future(std::move(sender) | ex::let_stopped([] {
                                  return ex::just_error(std::make_exception_ptr(
                                      std::runtime_error("cancelled")));
                                }));

  auto value = future.get();
  ASSERT_TRUE(value);
  EXPECT_EQ(42, *value);
}

int hpx_main() {
  test_result = RUN_ALL_TESTS();
  return hpx::local::finalize();
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  hpx::local::init_params init_args;
  init_args.cfg = {"hpx.os_threads=2"};
  const int hpx_result = hpx::local::init(hpx_main, argc, argv, init_args);
  return hpx_result == 0 ? test_result : hpx_result;
}
