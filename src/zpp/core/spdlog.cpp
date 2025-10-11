/**
 * @file spdlog.ipp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-06-25
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <vector>
#include <cstdarg>

#include <zpp/namespace.h>
#include <zpp/spdlog.h>
#include <zpp/spd_log.h>
#include <spdlog/cfg/env.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

std::shared_ptr<spdlog::logger> g_spd_log{nullptr};

NSB_ZPP

#if ENABLE_SPD_OS
std::ostringstream spd_os;

spd_infomartion_t inf;
spd_warning_t war;
spd_error_t err;
#endif


void spd_trace(int level, const char* file, int line, const char *fn, const char *msg, ...){
    thread_local std::array<char, 4096> buffer;
    va_list arglist;
    va_start(arglist, msg);
    int result = vsnprintf(buffer.data(), buffer.size() - 1, msg, arglist);
    va_end(arglist);
    //if (result >= 0 && result < static_cast<int>(buffer.size())) {buffer[result] = '\0';} else {buffer[buffer.size() - 1] = '\0';}
    buffer[result] = '\0';
    spd_logx(spdlog::source_loc{file, line, fn}, (spdlog::level::level_enum)level, fmt::runtime("{}"),buffer.data());
}
NSE_ZPP
