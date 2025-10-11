#include <gtest/gtest.h>
#include <zpp/coro/awaiter.h>

using namespace z::coro;

TEST(AwaiterExamples, ProducerReturnsValue) {
  auto t = producer();
  EXPECT_TRUE(t.valid());
  // With default awaiters promise runs to completion; check stored result
  EXPECT_EQ(t.result(), 123);
}

TEST(AwaiterExamples, ProducerRunHelper) {
  EXPECT_EQ(producer_run(), 123);
}

TEST(AwaiterExamples, AwaiterDelayResume) {
  // This test verifies that awaiting worker_delay does not deadlock and
  // completes. We run a coroutine that co_awaits worker_delay and sets a flag.

  bool done = false;

  // lightweight coroutine that waits and then sets done=true
  auto waiter = [&]() -> task<void> {
    co_await awaiter_delay{std::chrono::milliseconds(10)};
    done = true;
  
    co_return;
  };

  auto wt = waiter();
  // give some time for background thread to resume coroutine
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_TRUE(done);
}
