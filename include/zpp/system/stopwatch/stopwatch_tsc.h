#pragma once

#include <zpp/namespace.h>
#include <zpp/system/tsc.h>
#include "stopwatch.h"

NSB_ZPP
/**
 * @brief Timestamp Counter (TSC) based clock adapter (relaxed read).
 *
 * This adapter provides high-resolution time points based on the processor's
 * Time Stamp Counter (TSC). It is intended to be used with the generic
 * `stopwatch` template and offers very low overhead on supported platforms.
 *
 * Semantics and requirements:
 * - Time points are represented as `tsc_t` (integer cycle counters).
 * - `now()` returns a `tsc_t` representing the current cycle counter.
 * - `elapsed_*` functions accept explicit start and end `tsc_t` values and
 *   return durations in the named units (ns/us/ms) as `duration_t`.
 * - `step_*` functions accept a `tsc_t& start`, compute the delta to now,
 *   update `start` to the current time, and return the measured duration.
 *
 * Notes:
 * - The relaxed variant uses non-serializing TSC reads (RDTSC-like). It is
 *   faster but does not provide strict ordering guarantees relative to
 *   instruction execution; use the serialized variant (`tsc_s`) when
 *   ordering is required.
 * - Converting cycles to wall-clock time uses `tsc_to_ns()` / `elapsed_ns`
 *   helpers which may consult a cached cycles->ns factor when available.
 */
class tsc_r{
public:
    tsc_r() = default;
    ~tsc_r() = default;

    static inline tsc_t now() noexcept{
        return tsc_now_r();
    }

    static inline duration_t elapsed_ns(tsc_t start, tsc_t end) noexcept{
        return z::elapsed_ns(start, end);
    }
    static inline duration_t step_ns(tsc_t& start) noexcept{
        const tsc_t cycles = step_cycles_r(start);
        return tsc_to_ns(cycles);
    }

    static inline duration_t elapsed_us(tsc_t start, tsc_t end) noexcept{
        return z::elapsed_ns(start, end) / 1000;
    }
    static inline duration_t step_us(tsc_t& start) noexcept{
        const tsc_t cycles = step_cycles_r(start);
        return tsc_to_ns(cycles) / 1000;
    }

    static inline duration_t elapsed_ms(tsc_t start, tsc_t end) noexcept{
        return z::elapsed_ns(start, end) / 1000000;
    }
    static inline duration_t step_ms(tsc_t& start) noexcept{
        const tsc_t cycles = step_cycles_r(start);
        return tsc_to_ns(cycles) / 1000000;
    }
};

/**
 * @brief Timestamp Counter adapter using serialized reads.
 *
 * This variant performs serialized reads (for example via RDTSCP) to ensure
 * ordering with respect to instruction execution. It trades a small amount of
 * additional overhead for stronger ordering guarantees, which can be important
 * for certain profiling scenarios.
 */
class tsc_s{
public:
    tsc_s() = default;
    ~tsc_s() = default;

    static inline tsc_t now() noexcept{
        return tsc_now_s();
    }

    static inline duration_t elapsed_ns(tsc_t start, tsc_t end) noexcept{
        return z::elapsed_ns(start, end);
    }
    static inline duration_t step_ns(tsc_t& start) noexcept{
        const tsc_t cycles = step_cycles_s(start);
        return tsc_to_ns(cycles);
    }

    static inline duration_t elapsed_us(tsc_t start, tsc_t end) noexcept{
        return z::elapsed_ns(start, end) / 1000;
    }
    static inline duration_t step_us(tsc_t& start) noexcept{
        const tsc_t cycles = step_cycles_r(start);
        return tsc_to_ns(cycles) / 1000;
    }

    static inline duration_t elapsed_ms(tsc_t start, tsc_t end) noexcept{
        return z::elapsed_ns(start, end) / 1000000;
    }
    static inline duration_t step_ms(tsc_t& start) noexcept{
        const tsc_t cycles = step_cycles_r(start);
        return tsc_to_ns(cycles) / 1000000;
    }
};

using tsc = tsc_r;
using stopwatch_tsc = stopwatch<tsc, tsc_t>;
using stopwatch_tsc_ref = stopwatch_ref<tsc, tsc_t>;
//using stopwatch_tsc_r = stopwatch<tsc_r, tsc_t>;
//using stopwatch_tsc_s = stopwatch<tsc_s, tsc_t>;
NSE_ZPP