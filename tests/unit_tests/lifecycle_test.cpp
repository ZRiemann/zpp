#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include <zpp/core/lifecycle.h>

using namespace std::chrono_literals;

namespace {

TEST(Lifecycle, StartsAcquiresQuiescesAndStops) {
  z::lifecycle lifecycle;
  EXPECT_EQ(lifecycle.current_state(), z::lifecycle::state::stopped);
  EXPECT_EQ(lifecycle.active_count(), 0U);
  EXPECT_FALSE(lifecycle.accepting());
  EXPECT_TRUE(lifecycle.drained());

  ASSERT_TRUE(lifecycle.start());
  EXPECT_EQ(lifecycle.current_state(), z::lifecycle::state::running);
  EXPECT_TRUE(lifecycle.accepting());
  EXPECT_FALSE(lifecycle.drained());

  auto first = lifecycle.try_acquire();
  auto second = lifecycle.try_acquire();
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(lifecycle.active_count(), 2U);

  lifecycle.quiesce();
  EXPECT_EQ(lifecycle.current_state(), z::lifecycle::state::quiescing);
  EXPECT_FALSE(lifecycle.accepting());
  EXPECT_FALSE(lifecycle.try_acquire());
  EXPECT_FALSE(lifecycle.wait_drained_for(1ms));

  first.release();
  EXPECT_EQ(lifecycle.active_count(), 1U);
  EXPECT_FALSE(lifecycle.drained());

  second.release();
  EXPECT_TRUE(lifecycle.wait_drained_for(100ms));
  EXPECT_TRUE(lifecycle.drained());

  EXPECT_TRUE(lifecycle.stop());
  EXPECT_EQ(lifecycle.current_state(), z::lifecycle::state::stopped);
  EXPECT_TRUE(lifecycle.drained());
}

TEST(Lifecycle, GuardMoveTransfersExactlyOneActivity) {
  z::lifecycle lifecycle;
  ASSERT_TRUE(lifecycle.start());

  auto first = lifecycle.try_acquire();
  ASSERT_TRUE(first);
  EXPECT_EQ(lifecycle.active_count(), 1U);

  z::lifecycle::guard second{std::move(first)};
  EXPECT_FALSE(first);
  EXPECT_TRUE(second);
  EXPECT_EQ(lifecycle.active_count(), 1U);

  z::lifecycle::guard third;
  third = std::move(second);
  EXPECT_FALSE(second);
  EXPECT_TRUE(third);
  EXPECT_EQ(lifecycle.active_count(), 1U);

  lifecycle.quiesce();
  third.release();
  EXPECT_TRUE(lifecycle.wait_drained_for(100ms));
  EXPECT_TRUE(lifecycle.stop());
}

TEST(Lifecycle, StopRequiresQuiescingAndDrain) {
  z::lifecycle lifecycle;
  ASSERT_TRUE(lifecycle.start());

  EXPECT_FALSE(lifecycle.stop());

  auto activity = lifecycle.try_acquire();
  ASSERT_TRUE(activity);
  lifecycle.quiesce();
  EXPECT_FALSE(lifecycle.stop());

  activity.release();
  ASSERT_TRUE(lifecycle.wait_drained_for(100ms));
  EXPECT_TRUE(lifecycle.stop());
  EXPECT_TRUE(lifecycle.stop());
}

TEST(Lifecycle, SupportsRestartAfterCompleteStop) {
  z::lifecycle lifecycle;

  for (int cycle = 0; cycle < 3; ++cycle) {
    ASSERT_TRUE(lifecycle.start());
    auto activity = lifecycle.try_acquire();
    ASSERT_TRUE(activity);
    lifecycle.quiesce();
    activity.release();
    ASSERT_TRUE(lifecycle.wait_drained_for(100ms));
    ASSERT_TRUE(lifecycle.stop());
  }
}

TEST(Lifecycle, ConcurrentAcquireCannotCrossQuiesceBoundary) {
  z::lifecycle lifecycle;
  ASSERT_TRUE(lifecycle.start());

  std::atomic<bool> go{false};
  std::atomic<std::uint64_t> admitted{0};
  std::atomic<std::uint64_t> rejected{0};
  std::vector<std::thread> workers;
  workers.reserve(8);

  for (int index = 0; index < 8; ++index) {
    workers.emplace_back([&] {
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      for (;;) {
        auto activity = lifecycle.try_acquire();
        if (!activity) {
          rejected.fetch_add(1, std::memory_order_relaxed);
          return;
        }
        admitted.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::yield();
      }
    });
  }

  go.store(true, std::memory_order_release);
  while (admitted.load(std::memory_order_acquire) < 100) {
    std::this_thread::yield();
  }

  lifecycle.quiesce();

  for (auto &worker : workers) {
    worker.join();
  }

  EXPECT_EQ(rejected.load(std::memory_order_relaxed), workers.size());
  EXPECT_FALSE(lifecycle.try_acquire());
  EXPECT_TRUE(lifecycle.wait_drained_for(1s));
  EXPECT_EQ(lifecycle.active_count(), 0U);
  EXPECT_TRUE(lifecycle.stop());
}

} // namespace
