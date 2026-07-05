#pragma once

#include <atomic>
#include <memory>
#include <zpp/namespace.h>
#include <zpp/nng/task.h>
#include <zpp/system/timer.hpp>
#include <zpp/types.h>

NSB_ZPP
class task_foo;

using foo_t = nng::task<task_foo>;
using foo_pool_t = nng::task_pool<task_foo>;
/**
 * @class foo task use nng task
 * @code
 * // init task_foo::pool
 * nng::task<task_foo>* t{nullptr}
 * if(task_foo::pool->acquire(t)){
 *   // set current task `t` work context...
 *   t.dispatch();
 * }
 * @endcode
 */
class task_foo {
public:
  task_foo() = default;
  ~task_foo() = default;

  err_t work(int state) noexcept;

public: // test throughput
  static std::atomic_int _count;
  static z::timer<> _stopwatch;

public:
  // something others, like working context...
  static constexpr size_t capacity = 64;
  static constexpr int test_num = 10000000;
  static void init_pool();
  static void fini_pool() noexcept;

public:
  static std::unique_ptr<foo_pool_t> pool;
};
NSE_ZPP
