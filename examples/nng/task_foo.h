#pragma once

#include <zpp/namespace.h>
#include <moodycamel/obj_pool.h>
#include <zpp/types.h>
#include <zpp/nng/task.h>
#include <atomic>
#include <zpp/system/time.h>

USE_CAMEL

NSB_ZPP
class task_foo;

using foo_t = nng::task<task_foo*>;
/**
 * @class foo task use nng task
 * @code
 * // init task_foo::pool
 * nng::task<task_foo>* t{nullptr}
 * if(task_foo::pool.pop(t)){
 *   // set current task `t` work context...
 *   t.dispatch();
 * }
 * @endcode
 */
class task_foo{
public:
    task_foo() = default;
    ~task_foo() = default;

    err_t work(int state);
    void release(foo_t* task){
        pool.push(task);
    }
public: // test throughput
    static std::atomic_int _count;
    static z::time _stopwatch;
public:
    // something others, like working context...
    static constexpr size_t capacity = 64;
    static constexpr int test_num = 10000000;
    static void init_pool();
public:
    static obj_pool<foo_t> pool;
};
NSE_ZPP