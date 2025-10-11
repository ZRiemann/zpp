#pragma once

#include "defs.h"

NSB_NNG
/**
 * @class listener
 * @brief 
 * @note nng_listener_create*() 
 * 函数用于创建一个新的监听器（listener），但不会立即启动监听。
 * 这与 nng_listen 函数的主要区别在于，nng_listen 会创建并立即启动监听器。
 * 1. 需要在启动监听前配置高级选项
 * 2. 延迟启动监听器
 * 3. 动态管理多个监听器
 * 4. 创建安全监听端点
 */
class listener{
public:
};
NSE_NNG
