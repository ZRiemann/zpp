#pragma once
/**
 * @file zpp/spd_log.h
 * @note CUDA-safe printf logging wrapper. This header intentionally avoids
 * including spdlog/fmt heavy headers; use zpp/spdlog.h from normal C++ code.
 */
#include "namespace.h"

NSB_ZPP
namespace log {
bool should_trace_printf(int level) noexcept;
void trace_printf(int level, const char *file, int line, const char *fn,
                  const char *msg, ...);
} // namespace log
NSE_ZPP

#define spd2_log_if(level, fmt, ...)                                           \
  (z::log::should_trace_printf(level)                                          \
       ? z::log::trace_printf(level, __FILE__, __LINE__, __FUNCTION__, fmt,    \
                              ##__VA_ARGS__)                                   \
       : (void)0)

#define spd2dbg(fmt, ...) spd2_log_if(1, fmt, ##__VA_ARGS__)
#define spd2inf(fmt, ...) spd2_log_if(2, fmt, ##__VA_ARGS__)
#define spd2war(fmt, ...) spd2_log_if(3, fmt, ##__VA_ARGS__)
#define spd2err(fmt, ...) spd2_log_if(4, fmt, ##__VA_ARGS__)
#define spd2crt(fmt, ...) spd2_log_if(5, fmt, ##__VA_ARGS__)
#define spd2off(fmt, ...) spd2_log_if(6, fmt, ##__VA_ARGS__)
