#pragma once

#include <zpp/hpx/exec/detail/hpx_compat.hpp>

namespace z::hpx::exec {

/**
 * @warning starts_on/continues_on/on/transfer may introduce a real scheduling
 * boundary (queueing, worker wake-up, scheduler/executor switch, and possible
 * tail-latency amplification). Avoid switching scheduler at every stage only
 * for pipeline shape consistency.
 *
 * Prefer selecting one scheduler at pipeline entry and switch only when
 * crossing resource domains (for example: NNG I/O -> HPX CPU pool -> blocking
 * database I/O pool).
 */
using detail::continues_on;
using detail::let_error;
using detail::let_stopped;
using detail::let_value;
/** @copydoc continues_on */
using detail::on;
/** @copydoc continues_on */
using detail::starts_on;
/**
 * @warning Chaining many tiny stages with then() can be counterproductive for
 * performance. When each stage does only a few CPU instructions, completion
 * propagation and operation-state overhead may exceed useful work.
 *
 * Prefer coarser task boundaries that align with complete business stages,
 * especially for message-processing paths.
 */
using detail::then;
/** @copydoc continues_on */
using detail::transfer;
using detail::upon_error;
using detail::upon_stopped;
using detail::when_all;

} // namespace z::hpx::exec
