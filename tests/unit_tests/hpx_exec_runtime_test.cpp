#include <zpp/hpx/exec/exec.hpp>

#include <gtest/gtest.h>

#include <hpx/init.hpp>
#include <hpx/thread.hpp>

#include <tuple>

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
