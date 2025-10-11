#include "task_foo.h"
#include <zpp/spdlog.h>
#include <zpp/error.h>

USE_ZPP

obj_pool<foo_t> task_foo::pool;
std::atomic_int task_foo::_count(0);
z::time task_foo::_stopwatch;

void task_foo::init_pool(){
    foo_t* t{nullptr};
    task_foo *f{nullptr};
    for(size_t i = 0; i < capacity; ++i){
        f = new task_foo();
        t = new foo_t(f);
        pool.push(t);
    }
}
err_t task_foo::work(int state){
    //spd_inf("task[{}] foo working... state[{}]", fmt::ptr(this), state);
#if 0
    if(0 == _count){
        _stopwatch.update();
    }
    int count = _count.fetch_add(1) + 1;

    if(!(count & 0xffff)){
        time_t timespan = _stopwatch.elapsed_ms();
        spd_inf("foo working... tasks[{}] time[{}] avg:{}(tasks/sec)", count, timespan, (count / timespan) * 1000);
    }
#else
    int count = _count.fetch_add(1) + 1;
    if(1 == count){
        _stopwatch.update();
    }
    if(test_num == count){
        time_t timespan = _stopwatch.elapsed_ms();
        spd_inf("foo working... tasks:{} timespan:{}(ms) avg:{}(tasks/sec)", count, timespan, (count / timespan) * 1000);
    }
#endif
    return ERR_OK;
}
