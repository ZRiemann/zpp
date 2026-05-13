/**
 * @file spdlog.cpp
 * @brief Native-spdlog implementation for zpp logging initialization.
 */
#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string_view>
#include <vector>

#include <zpp/spd_log.h>
#include <zpp/spdlog.h>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

std::shared_ptr<spdlog::details::thread_pool> g_spd_pool;

std::shared_ptr<spdlog::logger> make_fallback_logger()
{
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("zpp", sink);
    logger->set_level(spdlog::level::off);
    return logger;
}

spdlog::level::level_enum normalize_level(int level)
{
    switch (level) {
    case 0:
        return spdlog::level::trace;
    case 1:
        return spdlog::level::debug;
    case 2:
        return spdlog::level::info;
    case 3:
        return spdlog::level::warn;
    case 4:
        return spdlog::level::err;
    case 5:
        return spdlog::level::critical;
    case 6:
        return spdlog::level::off;
    default:
        return spdlog::level::info;
    }
}

std::vector<spdlog::sink_ptr> make_sinks(const z::log::config& cfg)
{
    std::vector<spdlog::sink_ptr> sinks;
    if (cfg.console) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (cfg.rotating_file) {
        const auto max_size = cfg.max_file_size_mb * 1024 * 1024;
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            cfg.file_name, max_size, cfg.max_files, cfg.rotate_on_open));
    }
    if (sinks.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
    }

    const std::string pattern = cfg.pattern.empty()
        ? "%d %H:%M:%S.%f %t %^%L%$ %v\t%g:%#"
        : cfg.pattern;
    for (auto& sink : sinks) {
        sink->set_pattern(pattern);
    }
    return sinks;
}

void trace_vprintf(int level,
                   const char* file,
                   int line,
                   const char* fn,
                   const char* msg,
                   va_list args)
{
    if (!msg) {
        g_spd_log->log(spdlog::source_loc{file, line, fn}, normalize_level(level), "");
        return;
    }

    thread_local std::array<char, 4096> buffer;
    va_list args_copy;
    va_copy(args_copy, args);
    const int result = std::vsnprintf(buffer.data(), buffer.size(), msg, args_copy);
    va_end(args_copy);

    if (result < 0) {
        g_spd_log->log(spdlog::source_loc{file, line, fn}, normalize_level(level), "[format error]");
        return;
    }

    const std::size_t used = result >= static_cast<int>(buffer.size())
        ? buffer.size() - 1
        : static_cast<std::size_t>(result);
    buffer[used] = '\0';
    g_spd_log->log(spdlog::source_loc{file, line, fn},
                   normalize_level(level),
                   "{}",
                   std::string_view{buffer.data(), used});
}

} // namespace

std::shared_ptr<spdlog::logger> g_spd_log{make_fallback_logger()};

NSB_ZPP
namespace log {

void init(const config& cfg)
{
    auto sinks = make_sinks(cfg);
    if (cfg.async) {
        g_spd_pool = std::make_shared<spdlog::details::thread_pool>(cfg.queue_size, 1);
        g_spd_log = std::make_shared<spdlog::async_logger>(
            cfg.logger_name, sinks.begin(), sinks.end(), g_spd_pool, spdlog::async_overflow_policy::block);
    } else {
        g_spd_pool.reset();
        g_spd_log = std::make_shared<spdlog::logger>(cfg.logger_name, sinks.begin(), sinks.end());
    }

    g_spd_log->set_level(cfg.level);
    g_spd_log->flush_on(cfg.flush_level);
    spdlog::set_default_logger(g_spd_log);
    if (cfg.flush_interval.count() > 0) {
        spdlog::flush_every(cfg.flush_interval);
    }
    spdlog::cfg::load_env_levels();
}

void shutdown() noexcept
{
    try {
        auto logger = std::move(g_spd_log);
        auto pool = std::move(g_spd_pool);
        if (logger) {
            logger->flush();
        }
        spdlog::shutdown();
        pool.reset();
        g_spd_log = make_fallback_logger();
    } catch (...) {
        g_spd_log = make_fallback_logger();
        g_spd_pool.reset();
    }
}

void set_level(spdlog::level::level_enum level)
{
    g_spd_log->set_level(level);
}

void trace_printf(int level, const char* file, int line, const char* fn, const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    trace_vprintf(level, file, line, fn, msg, args);
    va_end(args);
}

} // namespace log

NSE_ZPP
