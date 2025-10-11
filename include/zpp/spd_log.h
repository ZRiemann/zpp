#pragma once
/**
 * @file zpp/spd_log.h
 * @note 引入中间函数，解决不能编译直接调用spdlog的问题。CUDA编程时编译失败处理方案。
 * spd2inf("hello %d %f", 1, 3.14);
 */
#include "namespace.h"

NSB_ZPP
void spd_trace(int level, const char* file, int line, const char *fn, const char *msg, ...);
NSE_ZPP

#define spd2dbg(fmt, ...) z::spd_trace(1, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2inf(fmt, ...) z::spd_trace(2, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2war(fmt, ...) z::spd_trace(2, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2err(fmt, ...) z::spd_trace(4, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2crt(fmt, ...) z::spd_trace(5, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define spd2off(fmt, ...) z::spd_trace(6, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)