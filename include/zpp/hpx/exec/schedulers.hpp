#pragma once

#include <zpp/hpx/exec/detail/hpx_compat.hpp>

namespace z::hpx::exec {

/** @brief Scheduler backed by the active HPX thread pool. */
using thread_pool_scheduler = detail::thread_pool_scheduler;

} // namespace z::hpx::exec
