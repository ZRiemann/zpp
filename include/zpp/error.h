#ifndef __ZPP_ERROR_H__
#define __ZPP_ERROR_H__

#include "types.h"

NSB_ZPP

const char *str_err(err_t);

constexpr err_t ERR_MASK = 0x7f000000;
constexpr err_t ERR_OK = 0;
constexpr err_t ERR_FAIL = ERR_MASK | 1;
constexpr err_t ERR_NOT_SUPPORT = ERR_MASK | 2;
constexpr err_t ERR_MEM_INSUFFICIENT = ERR_MASK | 3;
constexpr err_t ERR_OUT_OF_RANGE = ERR_MASK | 4;
constexpr err_t ERR_NOT_EXIST = ERR_MASK | 5;
constexpr err_t ERR_EXIST = ERR_MASK | 6;
constexpr err_t ERR_PARAM_INVALID = ERR_MASK | 7;
constexpr err_t ERR_AGAIN = ERR_MASK | 8;
constexpr err_t ERR_TIMEOUT = ERR_MASK | 9;
constexpr err_t ERR_END = ERR_MASK | 10;
NSE_ZPP

#endif
