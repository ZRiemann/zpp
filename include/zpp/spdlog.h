/**
 * @file spdlog.h
 * @author Z.Riemann
 * @brief 
 * @version 0.1
 * @date 2022-06-15
 * 
 * @copyright Copyright (c) 2022
 * @code {.usage}
 * #include <zpp/config.h> // define the global spd instance
 * #include <zpp/3rd/spdlog.h>
 * // initialize spdlog env
 * int main(int argc, char **argv){
 *      z::spdguard guard;
 *      guard.append_console_sink()
 *          //.append_rotating_file()
 *          .build_async();
 *      spd_inf("hello world!");
 * }
 * @endcode
 * 
 */
#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#ifdef FMT_USE_INT128
#undef FMT_USE_INT128
#define FMT_USE_INT128 0
#endif

#include <sstream>

#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include "namespace.h"

extern std::shared_ptr<spdlog::logger> g_spd_log;
class spdguard;

#define spd_begin(x) g_spd_log->enable_backtrace(x)
#define spd_end g_spd_log->dump_backtrace

template <typename... Args>
inline void spd_logx(spdlog::source_loc source,
                spdlog::level::level_enum lvl,
                spdlog::format_string_t<Args...> fmt,
                Args &&...args) {
    g_spd_log->log(source, lvl, fmt, std::forward<Args>(args)...);
}
#define spd_inf(...)  spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, __VA_ARGS__)
#define spd_err(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, __VA_ARGS__)
#define spd_war(...)  spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, __VA_ARGS__)
#define spd_dbg(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, __VA_ARGS__)
#define spd_trc(...) spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, __VA_ARGS__)

#ifdef _DEBUG
#define spd_mark() g_spd_log->info("{}:{}", __PRETTY_FUNCTION__, __LINE__)
#else
#define spd_mark()
#endif

// Usingx the Comma as a Thounds Separator: fmt::format
// spd_inf("comma as {}", 100000); # 100,000
#define CTS(x) fmt::format(std::locale("en_US.UTF-8"), "{:L}", (x))

#define ENABLE_SPD_OS 0
#if ENABLE_SPD_OS
// 60 times slower then user defined formatter
// psd_os << "abc" << inf;
// but it no need to defined formatter, for simple output.
// os.view() needs std_c++23
extern std::ostringstream spd_os;

NSB_ZPP
struct spd_infomartion_t{
    friend std::ostream&
    operator<<(std::ostream& os, spd_infomartion_t &){
        std::ostringstream &oss{static_cast<std::ostringstream&>(os)};
        g_spd_log->info("{}", oss.view());
        oss.str("");
        return os;
    }
};

struct spd_warning_t{
    friend std::ostream&
    operator<<(std::ostream& os, spd_warning_t &){
        std::ostringstream &oss{static_cast<std::ostringstream&>(os)};
        g_spd_log->warn("{}", oss.view());
        oss.str("");
        return os;
    }
};

struct spd_error_t{
    friend std::ostream&
    operator<<(std::ostream& os, spd_error_t &){
        std::ostringstream &oss{static_cast<std::ostringstream&>(os)};
        g_spd_log->error("{}", oss.view());
        oss.str("");
        return os;
    }
};

extern spd_infomartion_t inf;
extern spd_warning_t war;
extern spd_error_t err;

NSE_ZPP

#endif

