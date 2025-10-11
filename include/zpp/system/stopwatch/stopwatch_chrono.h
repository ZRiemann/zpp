#pragma once

#include <zpp/namespace.h>
#include <chrono>
#include "stopwatch.h"

NSB_ZPP

/**
 * @brief Chrono-based adapter usable with `stopwatch`.
 *
 * This adapter wraps a `std::chrono` clock and exposes the small interface
 * expected by the generic `stopwatch` template: `now()`, `elapsed_*` and
 * `step_*` functions. Durations are returned as `duration_t` integers.
 *
 * @tparam ClockType A `std::chrono` clock type (e.g. `std::chrono::steady_clock`).
 */
template <typename ClockType>
class stopwatch_chrono_adapter{
public:
    using clock_type = ClockType;
    using time_point_type = std::chrono::time_point<clock_type>;

    static inline time_point_type now() noexcept{
        return clock_type::now();
    }

    static inline duration_t elapsed_ns(time_point_type start, time_point_type end) noexcept{
        return static_cast<duration_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
        );
    }

    static inline duration_t step_ns(time_point_type& start) noexcept{
        time_point_type now_tp = clock_type::now();
        const auto d = std::chrono::duration_cast<std::chrono::nanoseconds>(now_tp - start).count();
        start = now_tp;
        return static_cast<duration_t>(d);
    }

    static inline duration_t elapsed_us(time_point_type start, time_point_type end) noexcept{
        return static_cast<duration_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );
    }

    static inline duration_t step_us(time_point_type& start) noexcept{
        time_point_type now_tp = clock_type::now();
        const auto d = std::chrono::duration_cast<std::chrono::microseconds>(now_tp - start).count();
        start = now_tp;
        return static_cast<duration_t>(d);
    }

    static inline duration_t elapsed_ms(time_point_type start, time_point_type end) noexcept{
        return static_cast<duration_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        );
    }

    static inline duration_t step_ms(time_point_type& start) noexcept{
        time_point_type now_tp = clock_type::now();
        const auto d = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - start).count();
        start = now_tp;
        return static_cast<duration_t>(d);
    }
};

/**
 * @brief Default chrono-based stopwatch typedef using `steady_clock`.
 */
using stopwatch_chrono = stopwatch<stopwatch_chrono_adapter<std::chrono::steady_clock>, std::chrono::time_point<std::chrono::steady_clock>>;

NSE_ZPP