#include <zpp/hpx/exec/exec.hpp>

#include <gtest/gtest.h>

#include <hpx/future.hpp>

#include <exception>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ex = z::hpx::exec;

namespace {

using set_value_type = std::remove_cvref_t<decltype(ex::set_value)>;

struct multiple_value_alternatives_sender {
  using sender_concept =
      typename std::remove_cvref_t<decltype(ex::just())>::sender_concept;
  using completion_signatures =
      ex::completion_signatures<set_value_type(int), set_value_type(double)>;
};

template <typename Sender>
concept can_start_detached = requires(Sender &&sender) {
  ex::start_detached(std::forward<Sender>(sender));
};

} // namespace

static_assert(ex::future_compatible_sender<decltype(ex::just())>);
static_assert(ex::future_compatible_sender<decltype(ex::just(42))>);
static_assert(!ex::future_compatible_sender<decltype(ex::just(1, 2))>);
static_assert(!ex::future_compatible_sender<decltype(ex::just_stopped())>);
static_assert(
    !ex::future_compatible_sender<multiple_value_alternatives_sender>);
static_assert(can_start_detached<decltype(ex::just())>);
static_assert(!can_start_detached<
              decltype(ex::just_error(std::declval<std::exception_ptr>()))>);

TEST(HpxExec, TransfersValueThroughThen) {
  auto result = ex::sync_wait(ex::just(21) |
                              ex::then([](int value) { return value * 2; }));

  ASSERT_TRUE(result);
  EXPECT_EQ(42, std::get<0>(*result));
}

TEST(HpxExec, SupportsVoidSender) {
  auto result = ex::sync_wait(ex::just() | ex::then([] {}));

  ASSERT_TRUE(result);
  static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(*result)>> == 0);
}

TEST(HpxExec, PreservesMoveOnlyValue) {
  auto result =
      ex::sync_wait(ex::just(std::make_unique<int>(42)) |
                    ex::then([](std::unique_ptr<int> value) { return value; }));

  ASSERT_TRUE(result);
  ASSERT_TRUE(std::get<0>(*result));
  EXPECT_EQ(42, *std::get<0>(*result));
}

TEST(HpxExec, SupportsMoveOnlyCallable) {
  auto state = std::make_unique<int>(21);
  auto result = ex::sync_wait(ex::just(2) |
                              ex::then([state = std::move(state)](int value) {
                                return *state * value;
                              }));

  ASSERT_TRUE(result);
  EXPECT_EQ(42, std::get<0>(*result));
}

TEST(HpxExec, PropagatesException) {
  auto sender = ex::just() | ex::then([]() -> int {
                  throw std::runtime_error("hpx exec failure");
                });

  EXPECT_THROW(static_cast<void>(ex::sync_wait(std::move(sender))),
               std::runtime_error);
}

TEST(HpxExec, CombinesSendersWithWhenAll) {
  auto result = ex::sync_wait(ex::when_all(ex::just(20), ex::just(22)));

  ASSERT_TRUE(result);
  EXPECT_EQ(20, std::get<0>(*result));
  EXPECT_EQ(22, std::get<1>(*result));
}

TEST(HpxExec, ConvertsFutureToSender) {
  auto result = ex::sync_wait(ex::from_future(hpx::make_ready_future(42)));

  ASSERT_TRUE(result);
  EXPECT_EQ(42, std::get<0>(*result));
}

TEST(HpxExec, ConvertsVoidFutureToSender) {
  auto result = ex::sync_wait(ex::from_future(hpx::make_ready_future()));

  ASSERT_TRUE(result);
}

TEST(HpxExec, ConvertsMoveOnlyFutureValueToSender) {
  auto future = hpx::make_ready_future(std::make_unique<int>(42));
  auto result = ex::sync_wait(ex::from_future(std::move(future)));

  ASSERT_TRUE(result);
  ASSERT_TRUE(std::get<0>(*result));
  EXPECT_EQ(42, *std::get<0>(*result));
}

TEST(HpxExec, PropagatesFutureExceptionThroughSender) {
  auto future = hpx::make_exceptional_future<int>(
      std::make_exception_ptr(std::runtime_error("future failure")));

  EXPECT_THROW(
      static_cast<void>(ex::sync_wait(ex::from_future(std::move(future)))),
      std::runtime_error);
}

TEST(HpxExec, ConvertsSenderToFuture) {
  auto future = ex::into_future(ex::just(42));

  EXPECT_EQ(42, future.get());
}

TEST(HpxExec, ConvertsVoidSenderToFuture) {
  auto future = ex::into_future(ex::just());

  EXPECT_NO_THROW(future.get());
}

TEST(HpxExec, PropagatesSenderExceptionThroughFuture) {
  auto sender = ex::just() | ex::then([]() -> int {
                  throw std::runtime_error("sender failure");
                });
  auto future = ex::into_future(std::move(sender));

  EXPECT_THROW(static_cast<void>(future.get()), std::runtime_error);
}
