#include "task_foo.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>

USE_ZPP

std::unique_ptr<foo_pool_t> task_foo::pool;
std::atomic_int task_foo::_count(0);
z::timer<> task_foo::_stopwatch;

void task_foo::init_pool() { pool = std::make_unique<foo_pool_t>(capacity); }

void task_foo::fini_pool() noexcept { pool.reset(); }

err_t task_foo::work(int state) noexcept {
  (void)state;
  // spd_inf("task[{}] foo working... state[{}]", fmt::ptr(this), state);
#if 0
    if(0 == _count){
        _stopwatch.update();
    }
    int count = _count.fetch_add(1) + 1;

    if(!(count & 0xffff)){
        auto timespan = _stopwatch.elapsed_ms();
        spd_inf("foo working... tasks[{}] time[{}] avg:{}(tasks/sec)", count, timespan, (count / timespan) * 1000);
    }
#else
  int count = _count.fetch_add(1) + 1;
  if (1 == count) {
    _stopwatch.update();
  }
  if (test_num == count) {
    auto timespan = _stopwatch.elapsed_ms();
    spd_inf("foo working... tasks:{} timespan:{}(ms) avg:{}(tasks/sec)", count,
            timespan, (count / timespan) * 1000);
  }
#endif
  return ERR_OK;
}
