#pragma once

#include <zpp/namespace.h>
#include <atomic>
#include <chrono>
#include <cstdint>

NSB_ZPP
enum class timer_sync_mode {
    relaxed,    // 只保证原子安全，不承担跨数据同步
    acquire_release // update 用 release，读取用 acquire
};

template <timer_sync_mode Mode = timer_sync_mode::relaxed>
class timer {
public:
    using clock_type = std::chrono::steady_clock;
    using duration   = clock_type::duration;
    using time_point = clock_type::time_point;
    using rep        = duration::rep;

public:
    timer() noexcept
        : ticks_(now_ticks())
    {
    }

    explicit timer(time_point tp) noexcept
        : ticks_(tp.time_since_epoch().count())
    {
    }

    timer(const timer&) = delete;
    timer& operator=(const timer&) = delete;

    void update() noexcept
    {
        ticks_.store(now_ticks(), store_order());
    }

    std::int64_t elapsed_ms() const noexcept
    {
        return elapsed<std::chrono::milliseconds>();
    }

    /**
     * @brief Return elapsed time in seconds since construction or last update.
     *
     * @return Seconds elapsed according to `std::chrono::steady_clock`.
     */
    std::int64_t elapsed_sec() const noexcept
    {
        return elapsed<std::chrono::seconds>();
    }

    std::int64_t elapsed_us() const noexcept
    {
        return elapsed<std::chrono::microseconds>();
    }

    std::int64_t elapsed_ns() const noexcept
    {
        return elapsed<std::chrono::nanoseconds>();
    }

    time_point last_time() const noexcept
    {
        const auto ticks = ticks_.load(load_order());
        return time_point(duration(ticks));
    }

private:
    template <typename Duration>
    std::int64_t elapsed() const noexcept
    {
        const auto last = last_time();
        const auto diff = clock_type::now() - last;

        return std::chrono::duration_cast<Duration>(diff).count();
    }

    static rep now_ticks() noexcept
    {
        return clock_type::now().time_since_epoch().count();
    }

    static constexpr std::memory_order store_order() noexcept
    {
        if constexpr (Mode == timer_sync_mode::relaxed) {
            return std::memory_order_relaxed;
        } else {
            return std::memory_order_release;
        }
    }

    static constexpr std::memory_order load_order() noexcept
    {
        if constexpr (Mode == timer_sync_mode::relaxed) {
            return std::memory_order_relaxed;
        } else {
            return std::memory_order_acquire;
        }
    }

private:
    std::atomic<rep> ticks_;
};

NSE_ZPP
