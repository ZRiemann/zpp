#pragma once

#include <zpp/namespace.h>

NSB_HPX

#include <spdlog/sinks/base_sink.h>
#include <hpx/iostream.hpp>

/**
 * @file spdlog.hpp
 * @brief Custom spdlog sinks for HPX integration.
 */

NSB_HPX

#include <spdlog/sinks/base_sink.h>
#include <hpx/iostream.hpp>

/**
 * @class hpx_ostream_sink
 * @brief Redirects spdlog output to HPX's distributed console stream (hpx::cout).
 *
 * Use this sink to aggregate logs from multiple HPX localities (nodes) to the root console.
 *
 * @section Warning
 * **Performance Implication**: Logging here triggers network serialization and transmission to Locality 0.
 * High-frequency logging causes network congestion.
 *
 * @section Use Case
 * **Best Practice (Mixed Strategy)**:
 * - **INFO/DEBUG**: Use local file sinks independent on each node (Performance).
 * - **WARN/ERROR**: Use `hpx_sink` for real-time aggregation/monitoring.
 *
 * @code {.cpp}
 * #include <zpp/hpx/spdlog.hpp>
 * #include <spdlog/sinks/basic_file_sink.h>
 *
 * void setup_hpx_logger() {
 *     // 1. Primary Sink: Local File
 *     auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
 *         fmt::format("logs/server.{}.log", hpx::get_locality_id()), true);
 *     file_sink->set_level(spdlog::level::info);
 *
 *     // 2. Secondary Sink: HPX Console
 *     auto hpx_sink = std::make_shared<z::zhpx::hpx_sink_mt>();
 *     hpx_sink->set_level(spdlog::level::err);
 *
 *     // Combine
 *     std::vector<spdlog::sink_ptr> sinks {file_sink, hpx_sink};
 *     auto logger = std::make_shared<spdlog::logger>("hpx_multi", sinks.begin(), sinks.end());
 *     logger->set_pattern("%+ [Loc:%d] [Th:%t]");
 *     spdlog::set_default_logger(logger);
 * }
 * @endcode
 */
template<typename Mutex>
class hpx_ostream_sink : public spdlog::sinks::base_sink<Mutex>
{
protected:
    /**
     * @brief Formats and sends the log message to hpx::cout.
     * @param msg The log message to process.
     */
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        // Buffer for formatting
        spdlog::memory_buf_t formatted;
        
        // Apply the formatter (pattern) to the message
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        // Convert to string and send to HPX distributed stream.
        // usage of hpx::cout requires the HPX runtime to be active.
        // fmt::to_string copies the buffer to a std::string.
        hpx::cout << fmt::to_string(formatted) << std::flush;
    }

    /**
     * @brief Flushes the HPX stream.
     */
    void flush_() override 
    {
        hpx::cout << std::flush;
    }
};

using hpx_sink_mt = hpx_ostream_sink<std::mutex>;
using hpx_sink_st = hpx_ostream_sink<spdlog::details::null_mutex>;

NSE_HPX