#pragma once

#include <zpp/hpx/exec/detail/hpx_compat.hpp>

namespace z::hpx::exec {

using detail::just;
using detail::just_error;
using detail::just_stopped;
/**
 * @warning A sender pipeline is not zero-cost. For tiny work items,
 * schedule/operation-state/completion propagation overhead can dominate the
 * business logic itself.
 *
 * Prefer task granularity that covers a complete business-processing stage
 * (for example, one full message stage) instead of splitting every tiny helper
 * into an independently scheduled task.
 */
using detail::schedule;

} // namespace z::hpx::exec
