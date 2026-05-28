#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>
#include <zpp/error.h>

#include <hpx/thread.hpp>

#include <chrono>

NSB_HPX

/**
 * @class server
 * @brief zpp server adapter whose idle loop cooperates with the HPX scheduler.
 */
class server : public z::server{
public:
    /**
     * @brief Timer interval type used by the HPX server adapter.
     */
    using timer_interval_type = std::chrono::milliseconds;

    /**
     * @brief Construct an HPX-aware zpp server.
     * @param argc Application argument count.
     * @param argv Application argument values.
     * @param timer_interval Interval used by `timer()` between loop iterations.
     */
    server(int argc, char** argv,
        timer_interval_type timer_interval = timer_interval_type{1000})
        :z::server(argc, argv)
        ,_timer_interval(timer_interval){}

    /**
     * @brief Destroy the HPX-aware zpp server.
     */
    ~server() override = default;

    /**
     * @brief Set the cooperative timer interval used by `timer()`.
     * @param timer_interval New interval between loop iterations.
     */
    inline void set_timer_interval(timer_interval_type timer_interval) noexcept{
        _timer_interval = timer_interval;
    }

    /**
     * @brief Get the cooperative timer interval used by `timer()`.
     * @return The current timer interval.
     */
    [[nodiscard]] inline timer_interval_type timer_interval() const noexcept{
        return _timer_interval;
    }

    /**
     * @brief Keep the application alive without blocking the underlying OS worker thread.
     */
    inline err_t timer() override{
        hpx::this_thread::sleep_for(_timer_interval);
        return ERR_OK;
    }

    /**
     * @brief Continue the zpp loop by default.
     * @return Always `ERR_OK`.
     */
    inline err_t on_timer() override{
        return ERR_OK; // continue looping
    }

private:
    timer_interval_type _timer_interval;
};
NSE_HPX
