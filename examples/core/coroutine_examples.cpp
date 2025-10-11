#include "coroutine_examples.h"
#include <chrono>
#include <coroutine>
#include <thread>
#include <zpp/spdlog.h>
#include <exception>
#include <memory>
#include <future>
#include <zpp/coro/awaiter.h>
#include <zpp/system/sleep.h>

NSB_STATIC
// 简单 awaiter：在 await_suspend 中启动线程睡眠，完成后 resume
struct SleepFor {
  std::chrono::milliseconds dur;
  bool await_ready() const noexcept { return dur.count() <= 0; }

  // 接受 coroutine_handle<void>，在新线程 sleep 后 resume
  void await_suspend(std::coroutine_handle<> h) const {
    std::thread([h, d = dur]() mutable {
        spd_inf("thread sleep for {} ms", d.count());
        std::this_thread::sleep_for(d);
        spd_inf("thread wakeup {} ms", d.count());
        // 注意与风险
        // 不能并发地由多个线程同时对同一 handle 调用 resume()（需要同步）。
        // 而应 post 一个任务到那个线程的队列/事件循环，由目标线程执行 h.resume()。
        h.resume(); // 恢复协程执行
    }).detach();
  }

  void await_resume() const noexcept {}
};

// 使用库封装的 task + awaiter_delay 实现可等待的延迟协程
z::coro::task<void> example_sleep() {
  spd_inf("before sleep");
  co_await z::coro::awaiter_delay{std::chrono::milliseconds(200)};
  spd_inf("after sleep");
  co_return;
}

// 使用z::coro::awaiter封装 实现MyTask类似功能
z::coro::task<int> produce() {
  co_return 84;
}
z::coro::task<void> consumer() {
  int v = co_await produce(); // await 调用 produce 的结果
  spd_inf("got value from produce: {}", v);
  co_return;
}
NSE_STATIC

USE_APP

void coroutine_examples::coro_sleep(){
    spd_inf("coroutine_examples::coro_sleep()");
    // 启动并保留返回的 task 对象以确保协程帧存活
    auto t1 = example_sleep();
    auto t2 = consumer();
    auto t3 = produce();
#if 0
    // 使用一个小的等待协程（将完成后设置 promise），主线程同步等待 future
    std::promise<void> p;
    auto f = p.get_future();
    auto waiter = [&p, &t1, &t2]() -> z::coro::task<z::coro::promise_void<>> {
      co_await t1;
      co_await t2;
      p.set_value();
      co_return;
    }();
    // 阻塞直到 t1 和 t2 完成
    f.get();

    int i = t3.result(); // 获取 produce 的结果（已完成或已等待）
    spd_inf("produce() returned: {}", i);
#endif
    sleep_ms(500); // 等待后台协程完成   
}