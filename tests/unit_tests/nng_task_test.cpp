#include <zpp/nng/nng.h>
#include <zpp/nng/task.h>

#include <gtest/gtest.h>

#include <atomic>

namespace {

struct test_worker {
  std::atomic<int> *count{nullptr};
  std::atomic<int> *last_state{nullptr};

  z::err_t work(int state) noexcept {
    if (last_state) {
      last_state->store(state, std::memory_order_release);
    }
    if (count) {
      count->fetch_add(1, std::memory_order_acq_rel);
    }
    return z::ERR_OK;
  }
};

bool wait_for_count(const std::atomic<int> &count, int expected) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    if (count.load(std::memory_order_acquire) >= expected) {
      return true;
    }
    nng_msleep(10);
  }
  return false;
}

} // namespace

TEST(NngTask, PoolRejectsWhenEmpty) {
  z::nng::nng runtime;
  z::nng::task_pool<test_worker> pool(1);
  z::nng::task<test_worker> *first{nullptr};
  z::nng::task<test_worker> *second{nullptr};

  EXPECT_EQ(pool.capacity(), 1U);
  ASSERT_TRUE(pool.acquire(first));
  EXPECT_NE(first, nullptr);
  EXPECT_FALSE(pool.acquire(second));
  EXPECT_EQ(second, nullptr);

  EXPECT_TRUE(pool.release(first));
  EXPECT_TRUE(pool.acquire(second));
  EXPECT_EQ(second, first);
  EXPECT_TRUE(pool.release(second));
}

TEST(NngTask, DispatchReturnsTaskToPool) {
  z::nng::nng runtime;
  z::nng::task_pool<test_worker> pool(1);
  std::atomic<int> count{0};
  std::atomic<int> last_state{-1};
  z::nng::task<test_worker> *task{nullptr};

  ASSERT_TRUE(pool.acquire(task));
  ASSERT_NE(task, nullptr);
  task->worker().count = &count;
  task->worker().last_state = &last_state;

  ASSERT_TRUE(task->dispatch());
  ASSERT_TRUE(wait_for_count(count, 1));
  EXPECT_EQ(last_state.load(std::memory_order_acquire), NNG_OK);

  z::nng::task<test_worker> *recycled{nullptr};
  for (int attempt = 0; attempt < 100 && recycled == nullptr; ++attempt) {
    if (pool.acquire(recycled)) {
      break;
    }
    nng_msleep(10);
  }
  ASSERT_NE(recycled, nullptr);
  EXPECT_EQ(recycled, task);
  EXPECT_TRUE(pool.release(recycled));
}

TEST(NngTask, StopRejectsBorrowedTaskDispatch) {
  z::nng::nng runtime;
  z::nng::task_pool<test_worker> pool(1);
  std::atomic<int> count{0};
  std::atomic<int> last_state{-1};
  z::nng::task<test_worker> *task{nullptr};

  ASSERT_TRUE(pool.acquire(task));
  ASSERT_NE(task, nullptr);
  task->worker().count = &count;
  task->worker().last_state = &last_state;

  pool.stop();
  EXPECT_FALSE(task->dispatch());
  EXPECT_EQ(count.load(std::memory_order_acquire), 1);
  EXPECT_EQ(last_state.load(std::memory_order_acquire), NNG_ESTOPPED);

  z::nng::task<test_worker> *stopped_task{nullptr};
  EXPECT_FALSE(pool.acquire(stopped_task));
}
