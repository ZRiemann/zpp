#pragma once

#include <zpp/hpx/exec/detail/hpx_compat.hpp>

#include <exception>
#include <type_traits>
#include <utility>

namespace z::hpx::exec::detail {

template <typename... Types> struct adapter_type_list {};

template <typename Type> struct future_value_shape : std::false_type {};

template <> struct future_value_shape<adapter_type_list<>> : std::true_type {};

template <>
struct future_value_shape<adapter_type_list<adapter_type_list<>>>
    : std::true_type {};

template <typename Type>
struct future_value_shape<adapter_type_list<adapter_type_list<Type>>>
    : std::true_type {};

template <typename Type> struct future_error_shape : std::false_type {};

template <> struct future_error_shape<adapter_type_list<>> : std::true_type {};

template <>
struct future_error_shape<adapter_type_list<std::exception_ptr>>
    : std::true_type {};

template <typename Sender>
using future_value_types =
    value_types_of_t<std::remove_cvref_t<Sender>, empty_env, adapter_type_list,
                     adapter_type_list>;

template <typename Sender>
using future_error_types =
    error_types_of_t<std::remove_cvref_t<Sender>, empty_env, adapter_type_list>;

template <typename Sender>
concept future_compatible_sender =
    sender_in<std::remove_cvref_t<Sender>, empty_env> &&
    future_value_shape<future_value_types<Sender>>::value &&
    future_error_shape<future_error_types<Sender>>::value &&
    !sends_stopped<std::remove_cvref_t<Sender>, empty_env>;

} // namespace z::hpx::exec::detail

namespace z::hpx::exec {

/** @brief A sender that can be converted to one HPX future without loss. */
template <typename Sender>
concept future_compatible_sender = detail::future_compatible_sender<Sender>;

/**
 * @brief Converts an HPX future into a sender using HPX's official adapter.
 * @warning Crossing the future/sender boundary is relatively expensive.
 * Prefer staying within one async model on hot paths, and avoid per-message
 * or per-small-task conversions.
 */
template <typename Future>
  requires requires(Future &&future) {
    detail::as_sender(std::forward<Future>(future));
  }
constexpr auto from_future(Future &&future) noexcept(
    noexcept(detail::as_sender(std::forward<Future>(future)))) {
  return detail::as_sender(std::forward<Future>(future));
}

/**
 * @brief Converts an HPX future into a sender with a completion scheduler.
 * @warning Crossing the future/sender boundary is relatively expensive.
 * Prefer staying within one async model on hot paths, and avoid per-message
 * or per-small-task conversions.
 */
template <typename Future, typename Scheduler>
  requires requires(Future &&future, Scheduler &&scheduler) {
    detail::as_sender(std::forward<Future>(future),
                      std::forward<Scheduler>(scheduler));
  }
constexpr auto from_future(Future &&future, Scheduler &&scheduler) noexcept(
    noexcept(detail::as_sender(std::forward<Future>(future),
                               std::forward<Scheduler>(scheduler)))) {
  return detail::as_sender(std::forward<Future>(future),
                           std::forward<Scheduler>(scheduler));
}

/**
 * @brief Converts a non-stoppable, zero-or-one-value sender to an HPX future.
 * @warning Crossing the future/sender boundary is relatively expensive.
 * Prefer staying within one async model on hot paths, and avoid per-message
 * or per-small-task conversions.
 */
template <future_compatible_sender Sender>
constexpr auto into_future(Sender &&sender) noexcept(
    noexcept(detail::make_future(std::forward<Sender>(sender)))) {
  return detail::make_future(std::forward<Sender>(sender));
}

} // namespace z::hpx::exec
