#include <gtest/gtest.h>
#if 1
#include <zpp/folly/coro.h>
#include <folly/coro/Task.h>
#include <folly/coro/BlockingWait.h>

using namespace z::fo;

// simple producer returning int
static folly::coro::Task<int> good_producer() {
  co_return 7;
}

static folly::coro::Task<int> bad_producer() {
  throw std::runtime_error("boom");
  co_return 0;
}

TEST(CoTry, Success) {
  auto t = co_blocking_wait(co_try(good_producer()));
  EXPECT_FALSE(t.hasException());
  EXPECT_EQ(t.value(), 7);
}

TEST(CoTry, Exception) {
  auto t = co_blocking_wait(co_try(bad_producer()));
  EXPECT_TRUE(t.hasException());
}
#endif