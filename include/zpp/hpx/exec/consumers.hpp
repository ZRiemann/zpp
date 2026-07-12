#pragma once

#include <zpp/hpx/exec/detail/hpx_compat.hpp>

#include <type_traits>
#include <utility>

namespace z::hpx::exec::detail {

template <typename... Types> struct type_list {};

template <typename Type> struct is_empty_type_list : std::false_type {};

template <> struct is_empty_type_list<type_list<>> : std::true_type {};

template <typename Sender>
using detached_error_types =
    error_types_of_t<std::remove_cvref_t<Sender>, empty_env, type_list>;

template <typename Sender>
concept detachable_sender =
    sender_in<std::remove_cvref_t<Sender>, empty_env> &&
    is_empty_type_list<detached_error_types<Sender>>::value;

} // namespace z::hpx::exec::detail

namespace z::hpx::exec {

using detail::sync_wait;

/**
 * @brief Starts unscoped work whose completion cannot report an error.
 * @details The operation is not joined automatically and does not add a stop
 * token. Callers must ensure that unscoped lifetime is appropriate.
 */
template <detail::detachable_sender Sender>
void start_detached(Sender &&sender) {
  detail::start_detached(std::forward<Sender>(sender));
}

} // namespace z::hpx::exec
