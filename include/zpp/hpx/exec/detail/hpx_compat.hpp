#pragma once

#include <hpx/config.hpp>

#if !defined(HPX_HAVE_STDEXEC)
#error "z::hpx::exec requires the zeta_forge HPX build with stdexec enabled"
#endif

#include <hpx/execution/algorithms/as_sender.hpp>
#include <hpx/execution/algorithms/make_future.hpp>
#include <hpx/execution_base/stdexec_forward.hpp>
#include <hpx/executors/thread_pool_scheduler.hpp>

#include <utility>
#include <variant>

namespace z::hpx::exec::detail {

namespace hpx_exec = ::hpx::execution::experimental;

using hpx_exec::as_sender;
using hpx_exec::connect;
using hpx_exec::continues_on;
using hpx_exec::get_completion_signatures;
using hpx_exec::just;
using hpx_exec::just_error;
using hpx_exec::just_stopped;
using hpx_exec::let_error;
using hpx_exec::let_stopped;
using hpx_exec::let_value;
using hpx_exec::make_future;
using hpx_exec::on;
using hpx_exec::schedule;
using hpx_exec::set_error;
using hpx_exec::set_stopped;
using hpx_exec::set_value;
using hpx_exec::start;
using hpx_exec::starts_on;
using hpx_exec::sync_wait;
using hpx_exec::then;
using hpx_exec::transfer;
using hpx_exec::upon_error;
using hpx_exec::upon_stopped;
using hpx_exec::when_all;

using hpx_exec::empty_env;
using hpx_exec::thread_pool_scheduler;

template <typename... Signatures>
using completion_signatures = hpx_exec::completion_signatures<Signatures...>;

template <typename Sender, typename Environment = empty_env,
          template <typename...> typename Variant = std::variant>
using error_types_of_t =
    hpx_exec::error_types_of_t<Sender, Environment, Variant>;

template <typename Sender, typename Environment,
          template <typename...> typename Tuple,
          template <typename...> typename Variant>
using value_types_of_t =
    hpx_exec::value_types_of_t<Sender, Environment, Tuple, Variant>;

template <typename Sender, typename... Environments>
inline constexpr bool sends_stopped =
    hpx_exec::sends_stopped<Sender, Environments...>;

template <typename Type>
concept sender = hpx_exec::sender<Type>;

template <typename Type, typename... Environments>
concept sender_in = hpx_exec::sender_in<Type, Environments...>;

template <typename Sender, typename Receiver>
concept sender_to = hpx_exec::sender_to<Sender, Receiver>;

template <typename Type>
concept receiver = hpx_exec::receiver<Type>;

template <typename Receiver, typename Completions>
concept receiver_of = hpx_exec::receiver_of<Receiver, Completions>;

template <typename Type>
concept scheduler = hpx_exec::scheduler<Type>;

template <typename Type>
concept operation_state = hpx_exec::operation_state<Type>;

template <typename Sender> void start_detached(Sender &&sender) {
  hpx_exec::start_detached(std::forward<Sender>(sender));
}

} // namespace z::hpx::exec::detail
