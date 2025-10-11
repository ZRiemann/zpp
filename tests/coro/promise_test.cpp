#include <type_traits>
#include <utility>
#include <gtest/gtest.h>
#include <zpp/coro/promise.h>

// detection helpers
template <typename, typename = void>
struct has_return_value : std::false_type {};

template <typename P>
struct has_return_value<
    P, std::void_t<decltype(std::declval<P>().return_value(std::declval<int>()))>>
    : std::true_type {};

template <typename, typename = void>
struct has_return_void : std::false_type {};

template <typename P>
struct has_return_void<P, std::void_t<decltype(std::declval<P>().return_void())>>
    : std::true_type {};

TEST(PromiseTraits, ReturnValueAndVoid) {
  EXPECT_TRUE((has_return_value<z::coro::promise_result<int>>::value));
  EXPECT_FALSE((has_return_value<z::coro::promise_void<>>::value));

  EXPECT_TRUE((has_return_void<z::coro::promise_void<>>::value));
  EXPECT_FALSE((has_return_void<z::coro::promise_result<int>>::value));
}

