#pragma once

#include "defs.h"

NSB_NNG
/**
 * @class dialer
 * @note nng_dialer_create() 创建拨号器（dialer）但不立即启动连接的函数
 * 提供了更精细的控制，允许在启动连接前配置拨号器的各种选项。
 * 1. 需要在启动连接前配置高级选项
 * 2. 延迟启动连接
 * 3. 动态管理多个连接
 * 4. 创建安全连接
 * 5. 使用预解析的 URL
 * 1. 多协议客户端
 * 3. 连接重试与失败转移, 实现连接失败自动转移到备用服务器的客户端
 */
class dialer{
public:
    dialer(){

    }
    dialer(nng_dialer d):_dialer(d){

    }
    ~dialer(){
        nng_dialer_close(_dialer);
    }
public:
    nng_dialer _dialer{NNG_DIALER_INITIALIZER};
}
NSE_NNG