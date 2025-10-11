#pragma once

#include <zpp/core/engine/namespace.h>

/**
 * @brief asynchronous operator (aop)
 * @note 类继承版本异步操作，避免复杂的设计、扩展与维护问题
 *  与 scheduler_aop, thread_pool 配合实现高效的任务调度引擎
 * @note used by scheduler_aop
 */
NSB_ENGINE

class aop{
public:
    aop() = default;
    virtual ~aop() = default;

    virtual void operate() = 0;
private:
    // user datas
};
NSE_ENGINE
