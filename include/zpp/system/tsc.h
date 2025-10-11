#pragma once

#include <zpp/types.h>

#include <atomic>
#include <cstdint>
#include <limits>

#if defined(__i386__) || defined(__x86_64__)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif
#else
#error "timestamp counter helpers are only available on x86/x86_64"
#endif

NSB_ZPP

#if 0
// libzpp, libzpp_nng, libzpp_folly may have different translation units,
inline double s_tsc_freq{0.0}; // c++17, maybe not work well in some compilers
inline double& get_tsc_frequency() noexcept{
    //static double tsc_freq = 0.0; // c++11
    //return tsc_freq;
    return s_tsc_freq;
}
inline double& get_tsc_ns_per() noexcept{
    static double tsc_ns_per = 0.0;
    return tsc_ns_per;
}

inline double detect_tsc_frequency_hz(){return 0.0;}
inline bool tsc_init() noexcept{return true;}
#endif

/**
 * @brief Timestamp counter frequency detection.
 * @note
 * TSC: <10 cycles/ns, RDTSC:~100 cycles/ns, clock_gettime()/std::chrono::steady_clock: ~1000 cycles/ns
 * TSC only support x86/x86_64 platform and real system(not vm), other platform can use
 * clock_gettime() POSIX API. Can work on Linux, MacOS, *BSD. and VM.
 */
extern double s_tsc_frequency;
extern double s_tsc_ns_per;

inline double& get_tsc_frequency() noexcept{
    return s_tsc_frequency;
}
inline double& get_tsc_ns_per() noexcept{
    return s_tsc_ns_per;
}

/**
 * @brief Probe and initialize the TSC frequency used by this library.
 *
 * This function first checks whether a user override has already been set
 * via `set_timestamp_counter_frequency_hz()`. If not, it attempts to read
 * the CPU-reported TSC frequency (via CPUID leaves) and, on success, sets
 * the library-wide override so subsequent conversions use a stable value.
 *
 * @return `true` if a frequency is available (either previously overridden
 *         or successfully detected and installed); `false` if the CPU does
 *         not expose the required information and no override is present.
 */
bool tsc_init();

/**
 * @brief Read the time-stamp counter with minimum overhead.
 *
 * Invokes a bare RDTSC instruction (no serialization fences). Callers must
 * tolerate that surrounding instructions can be reordered around the read.
 */
inline tsc_t tsc_now_r() noexcept{
    return __rdtsc();
}

/**
 * @brief Read the time-stamp counter with LFENCE/RDTSCP serialization.
 *
 * Emits an LFENCE before the measurement point and uses RDTSCP to ensure
 * no following instructions execute before the counter value is captured.
 */
inline tsc_t tsc_now_s() noexcept{
    _mm_lfence();
    unsigned int aux;
    return __rdtscp(&aux);
}

/**
 * @brief Convert raw TSC cycles into nanoseconds using the known frequency.
 * @param cycles Difference between two readings of the timestamp counter.
 * @return Duration in nanoseconds (rounded), or 0 when frequency is unknown.
 */
inline duration_t tsc_to_ns(tsc_t cycles) noexcept{
    // Fast path: if we have a cached nanoseconds-per-cycle factor, use a
    // single multiply (hot path). Otherwise fall back to division-based
    // computation using the reported frequency.
    const double ns_per_cycle = get_tsc_ns_per();
    if (ns_per_cycle > 0.0) {
        const long double nanos = static_cast<long double>(cycles) * static_cast<long double>(ns_per_cycle);
        if (nanos <= 0.0L) {
            return 0;
        }
        if (nanos >= static_cast<long double>(std::numeric_limits<uint64_t>::max())) {
            return std::numeric_limits<uint64_t>::max();
        }
        return static_cast<duration_t>(nanos + 0.5L);
    }

    if (get_tsc_frequency() <= 0.0){
        return 0;
    }
    const long double numerator = static_cast<long double>(cycles) * 1'000'000'000.0L;
    const long double nanos = numerator / static_cast<long double>(get_tsc_frequency());
    if (nanos <= 0.0L){
        return 0;
    }
    if (nanos >= static_cast<long double>(std::numeric_limits<uint64_t>::max())){
        return std::numeric_limits<uint64_t>::max();
    }
    return static_cast<duration_t>(nanos + 0.5L);
}

inline duration_t elapsed_ns(tsc_t start, tsc_t end) noexcept{
    return tsc_to_ns(end - start);
}

inline duration_t elapsed_us(tsc_t start, tsc_t end) noexcept{
    return tsc_to_ns(end - start) / 1000;
}

inline duration_t elapsed_ms(tsc_t start, tsc_t end) noexcept{
    return tsc_to_ns(end - start) / 1'000'000;
}

/**
 * @brief Measure elapsed time and advance `start` (relaxed read).
 *
 * Reads the timestamp counter using the relaxed (RDTSC) variant, computes
 * the cycle difference `now - start`, converts to nanoseconds via
 * `tsc_to_ns`, updates `start` to `now`, and returns the elapsed time in
 * nanoseconds.
 *
 * @important Threading and race conditions:
 * - This function modifies the caller-provided `start` reference and is
 *   NOT thread-safe. Concurrent calls that share the same `start` will
 *   race and produce undefined or surprising results.
 * - To avoid unnecessary synchronization or locks, callers should ensure
 *   ownership of `start` is single-threaded (for example, thread-local or
 *   otherwise exclusively owned by the caller) rather than protecting it
 *   with mutexes around every timing operation.
 *
 * @note Use the serialized variant (`step_ns_s`) when stronger ordering is
 * required at the cost of higher overhead.
 *
 * @param start Reference to a previously-sampled TSC value; will be set to
 *              the current TSC on return.
 * @return Elapsed time in nanoseconds (rounded), or 0 if the TSC frequency
 *         is unknown or the elapsed time is below measurable resolution.
 */
/**
 * @brief Lightweight step that returns the raw cycle difference and advances
 *        `start` (relaxed read).
 *
 * Returning the cycle delta lets callers accumulate multiple small intervals
 * before converting to nanoseconds, which improves effective resolution and
 * avoids repeated floating-point math in hot loops.
 */
inline tsc_t step_cycles_r(tsc_t& start) noexcept{
    const tsc_t now = tsc_now_r();
    const tsc_t diff = now - start;
    start = now;
    return diff;
}

/**
 * @brief Lightweight step that returns the raw cycle difference and advances
 *        `start` (serialized read).
 */
inline tsc_t step_cycles_s(tsc_t& start) noexcept{
    const tsc_t now = tsc_now_s();
    const tsc_t diff = now - start;
    start = now;
    return diff;
}

/**
 * @brief Convenience wrappers that return elapsed nanoseconds.
 *
 * These call the corresponding `step_cycles_*` helpers and convert the
 * result to nanoseconds. Prefer using the cycle-returning variants in
 * extremely hot paths and batch conversion where possible.
 */
inline duration_t step_ns_r(tsc_t& start) noexcept{
    return tsc_to_ns(step_cycles_r(start));
}

inline duration_t step_ns_s(tsc_t& start) noexcept{
    return tsc_to_ns(step_cycles_s(start));
}
NSE_ZPP