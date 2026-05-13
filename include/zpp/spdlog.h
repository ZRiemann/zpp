/**
 * @file spdlog.h
 * @brief Thin zpp wrapper around the native spdlog hot path.
 */
#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#ifdef FMT_USE_INT128
#undef FMT_USE_INT128
#define FMT_USE_INT128 0
#endif

#include <chrono>
#include <cstddef>
#include <locale>
#include <memory>
#include <string>
#include <utility>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "namespace.h"

extern std::shared_ptr<spdlog::logger> g_spd_log;

NSB_ZPP
namespace log {

struct config {
    bool async{false};
    bool console{true};
    bool rotating_file{false};
    bool rotate_on_open{true};
    std::string file_name{"server.log"};
    std::size_t max_file_size_mb{32};
    std::size_t max_files{3};
    std::size_t queue_size{8192};
    std::chrono::seconds flush_interval{2};
    spdlog::level::level_enum level{spdlog::level::trace};
    spdlog::level::level_enum flush_level{spdlog::level::warn};
    std::string logger_name{"zpp"};
    std::string pattern{"%d %H:%M:%S.%f %t %^%L%$ %v\t%g:%#"};
};

void init(const config& cfg = config{});
void shutdown() noexcept;
void set_level(spdlog::level::level_enum level);

} // namespace log
NSE_ZPP

#define spd_begin(x) g_spd_log->enable_backtrace(x)
#define spd_end g_spd_log->dump_backtrace

template <typename... Args>
inline void spd_logx(spdlog::source_loc source,
                     spdlog::level::level_enum lvl,
                     spdlog::format_string_t<Args...> fmt,
                     Args&&... args) {
    g_spd_log->log(source, lvl, fmt, std::forward<Args>(args)...);
}

#define spd_inf(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, __VA_ARGS__)
#define spd_err(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, __VA_ARGS__)
#define spd_war(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, __VA_ARGS__)
#define spd_dbg(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, __VA_ARGS__)
#define spd_trc(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, __VA_ARGS__)

#ifdef _DEBUG
#define spd_mark() g_spd_log->info("{}:{}", __PRETTY_FUNCTION__, __LINE__)
#else
#define spd_mark()
#endif

#define CTS(x) fmt::format(std::locale("en_US.UTF-8"), "{:L}", (x))
