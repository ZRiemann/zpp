#pragma once

#include <zpp/namespace.h>
#include <zpp/types.h>

NSB_ZPP

/**
 * @brief Generic stopwatch template.
 *
 * This class template provides a small, low-overhead stopwatch abstraction
 * parameterized by a ClockType and a corresponding TimePointType. It is
 * intentionally minimal: construction captures the current time via
 * `ClockType::now()`, and public methods provide elapsed/step durations in
 * nanoseconds/microseconds/milliseconds as `duration_t`.
 *
 * Requirements for ClockType:
 * - static TimePointType now() noexcept;
 * - static duration_t elapsed_ns(TimePointType start, TimePointType end) noexcept;
 * - static duration_t step_ns(TimePointType& start) noexcept; (updates start)
 * - analogous `elapsed_us`/`step_us` and `elapsed_ms`/`step_ms` variants.
 *
 * The template does not perform any heap allocation or synchronization and is
 * suitable for use on hot paths. The units returned are platform-independent
 * integers in nanoseconds / microseconds / milliseconds respectively.
 *
 * @tparam ClockType   Clock adapter providing `now`, `elapsed_*` and `step_*`.
 * @tparam TimePointType The type returned by `ClockType::now()`.
 */
template <typename ClockType, typename TimePointType>
class stopwatch{
public:
    /**
     * @brief Construct and start the stopwatch at the current time.
     */
    stopwatch()
        :_time_point(ClockType::now()){
    }

    ~stopwatch() = default;

    /**
     * @brief Start or restart the stopwatch from now.
     *
     * This writes the current time returned by `ClockType::now()` into the
     * internal `_time_point` so subsequent `elapsed_*`/`step_*` calls are
     * relative to this new baseline.
     */
    void start(){
        _time_point = ClockType::now();
    }

    /**
     * @brief Return elapsed time in nanoseconds since construction or last start.
     *
     * This delegates to `ClockType::elapsed_ns(start, end)` using the stored
     * `_time_point` as the start and `ClockType::now()` as the end.
     *
     * @return duration in nanoseconds as `duration_t`.
     */
    inline duration_t elapsed_ns() noexcept {
        return ClockType::elapsed_ns(_time_point, ClockType::now());
    }

    /**
     * @brief Return time in nanoseconds since the stored `_time_point`, and
     *        update `_time_point` to now (a "step" operation).
     *
     * Useful for measuring intervals in a loop while resetting the baseline
     * on each measurement.
     *
     * @return duration in nanoseconds as `duration_t`.
     */
    inline duration_t step_ns() noexcept {
        return ClockType::step_ns(_time_point);
    }

    /**
     * @brief Elapsed time in microseconds since baseline.
     *
     * Delegates to `ClockType::elapsed_us(start, end)`.
     *
     * @return duration in microseconds as `duration_t`.
     */
    inline duration_t elapsed_us() noexcept {
        return ClockType::elapsed_us(_time_point, ClockType::now());
    }

    /**
     * @brief Step variant that returns microseconds and updates the baseline.
     *
     * @return duration in microseconds as `duration_t`.
     */
    inline duration_t step_us() noexcept {
        return ClockType::step_us(_time_point);
    }

    /**
     * @brief Elapsed time in milliseconds since baseline.
     *
     * Delegates to `ClockType::elapsed_ms(start, end)`.
     *
     * @return duration in milliseconds as `duration_t`.
     */
    inline duration_t elapsed_ms() noexcept {
        return ClockType::elapsed_ms(_time_point, ClockType::now());
    }

    /**
     * @brief Step variant that returns milliseconds and updates the baseline.
     *
     * @return duration in milliseconds as `duration_t`.
     */
    inline duration_t step_ms() noexcept {
        return ClockType::step_ms(_time_point);
    }
public:
    /**
     * @brief The stored time point used as the measurement baseline.
     *
     * This member is public to allow `stopwatch_ref` and external code to
     * reference or persist a baseline time directly. It is expected to be
     * identical to `TimePointType` (no extra invariants).
     */
    TimePointType _time_point;
};

/**
 * @brief Stopwatch that operates on an external time point reference.
 *
 * `stopwatch_ref` is a lightweight wrapper that uses a reference to an
 * externally-managed `TimePointType` (for example a shared or global
 * baseline). All operations are identical to `stopwatch` but act on the
 * referenced time point.
 *
 * @tparam ClockType     Clock adapter providing `now`, `elapsed_*` and `step_*`.
 * @tparam TimePointType The type of the referenced time point.
 */
template <typename ClockType, typename TimePointType>
class stopwatch_ref{
public:
    /**
     * @brief Construct a reference stopwatch from an existing time point.
     *
     * @param tp Reference to a `TimePointType` that will be used as baseline.
     */
    stopwatch_ref(TimePointType& tp)
        :_time_point(tp){
    }
    ~stopwatch_ref() = default;

    /**
     * @brief Update the referenced time point to the current time.
     */
    void start(){
        _time_point = ClockType::now();
    }

    /**
     * @brief Elapsed time in nanoseconds using the referenced baseline.
     *
     * Delegates to `ClockType::elapsed_ns(start, end)`.
     */
    inline duration_t elapsed_ns() noexcept {
        return ClockType::elapsed_ns(_time_point, ClockType::now());
    }

    /**
     * @brief Step variant using the referenced baseline; updates the reference.
     */
    inline duration_t step_ns() noexcept {
        return ClockType::step_ns(_time_point);
    }

    /**
     * @brief Elapsed time in microseconds using the referenced baseline.
     */
    inline duration_t elapsed_us() noexcept {
        return ClockType::elapsed_us(_time_point, ClockType::now());
    }

    /**
     * @brief Step variant in microseconds using the referenced baseline.
     */
    inline duration_t step_us() noexcept {
        return ClockType::step_us(_time_point);
    }

    /**
     * @brief Elapsed time in milliseconds using the referenced baseline.
     */
    inline duration_t elapsed_ms() noexcept {
        return ClockType::elapsed_ms(_time_point, ClockType::now());
    }

    /**
     * @brief Step variant in milliseconds using the referenced baseline.
     */
    inline duration_t step_ms() noexcept {
        return ClockType::step_ms(_time_point);
    }
public:
    /**
     * @brief Reference to the external baseline time point.
     */
    TimePointType& _time_point;
};

NSE_ZPP