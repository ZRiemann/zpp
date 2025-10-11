#include <gtest/gtest.h>
#include <zpp/coro/task.h>
#include <zpp/coro/awaiter.h>

using namespace z::coro;

// Simple producer used by multiple tests
static inline task<int> test_producer() {
  co_return 123;
}

TEST(TaskBasic, ConstructAndResult) {
  auto t = test_producer();
  EXPECT_TRUE(t.valid());
  EXPECT_EQ(t.result(), 123);
}

TEST(TaskMove, MoveSemantics) {
  auto t1 = test_producer();
  auto t2 = std::move(t1);
  EXPECT_FALSE(t1.valid());
  EXPECT_TRUE(t2.valid());
  EXPECT_EQ(t2.result(), 123);
}

TEST(TaskOwnership, ReleaseAndDestroy) {
  auto t = test_producer();
  auto h = t.release();
  EXPECT_FALSE(t.valid());
  // We must destroy the released handle to avoid leak
  if (h) h.destroy();
}

TEST(TaskOwnership, DetachLeavesNoOwner) {
  auto t = test_producer();
  t.detach();
  EXPECT_FALSE(t.valid());
  // intentionally not destroying the frame: detach semantics documented
}

TEST(TaskAwaitable, CoAwaitValueTask) {
  // producer that returns a small int
  auto async_producer = []() -> task<int> {
    co_return 7;
  };

  // consumer awaits the producer and returns value+1
  auto consumer = [&]() -> task<int> {
    int v = co_await async_producer();
    co_return v + 1;
  };

  auto c = consumer();
  EXPECT_TRUE(c.valid());
  EXPECT_EQ(c.result(), 8);
}

TEST(TaskAwaiter, AwaitRvalueVoid) {
  bool done = false;

  auto async_void = []() -> task<void> {
    co_await awaiter_delay{std::chrono::milliseconds(0)}; // immediate, no suspend
    co_return;
  };

  auto consumer = [&]() -> task<void> {
    co_await async_void(); // await rvalue task
    done = true;
    co_return;
  };

  auto c = consumer();
  // allow background executor to resume
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_TRUE(done);
}
