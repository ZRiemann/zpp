#pragma once

#include <zpp/hpx/exec/detail/hpx_compat.hpp>

namespace z::hpx::exec {

/** @brief A type that models the Sender concept. */
template <typename Type>
concept sender = detail::sender<Type>;

/** @brief A type that models the Receiver concept. */
template <typename Type>
concept receiver = detail::receiver<Type>;

/** @brief A receiver accepting every listed completion signature. */
template <typename Receiver, typename Completions>
concept receiver_of = detail::receiver_of<Receiver, Completions>;

/** @brief A sender connectable to the specified receiver. */
template <typename Sender, typename Receiver>
concept sender_to = detail::sender_to<Sender, Receiver>;

/** @brief A scheduler accepted by the HPX execution implementation. */
template <typename Type>
concept scheduler = detail::scheduler<Type>;

/** @brief A connected operation state that can be started. */
template <typename Type>
concept operation_state = detail::operation_state<Type>;

/** @brief Describes the completion operations supported by a sender. */
template <typename... Signatures>
using completion_signatures = detail::completion_signatures<Signatures...>;

using detail::connect;
using detail::get_completion_signatures;
using detail::set_error;
using detail::set_stopped;
using detail::set_value;
using detail::start;

} // namespace z::hpx::exec
