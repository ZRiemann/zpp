#pragma once
/**
 * @file zpp/spd_log.h
 * @note CUDA-safe printf logging wrapper. This header intentionally avoids
 * including spdlog/fmt heavy headers; use zpp/spdlog.h from normal C++ code.
 */
#include "namespace.h"

NSB_ZPP
namespace log {
void trace_printf(int level, const char* file, int line, const char* fn, const char* msg, ...);
} // namespace log
NSE_ZPP

#define spd2dbg(fmt, ...) z::log::trace_printf(1, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2inf(fmt, ...) z::log::trace_printf(2, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2war(fmt, ...) z::log::trace_printf(3, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2err(fmt, ...) z::log::trace_printf(4, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2crt(fmt, ...) z::log::trace_printf(5, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2off(fmt, ...) z::log::trace_printf(6, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
