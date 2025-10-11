#include "executor_example.h"
#include <zpp/spdlog.h>
#include <zpp/system/stopwatch/stopwatch_tsc.h>

#include <folly/futures/Future.h>
#include <folly/executors/InlineExecutor.h>
#include <folly/executors/ManualExecutor.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/SerialExecutor.h>
#include <folly/executors/QueuedImmediateExecutor.h>
#include <folly/synchronization/Baton.h>

extern "C" void c_func(int x){
    spd_inf("c_func({})", x);
}

extern "C" void c_fn(){
    int a = 3;
    int b = 4;
    int c = a + b;
    ++c;
    spd_inf("c_fn()");
}

void foo() { spd_inf("hello from foo"); }

struct S { void m(int v) { spd_inf("value: {}", v); } };
USE_ZPP

static void future_default_executor(){
    auto f = folly::makeFuture(1).thenValue([](int v) {
        spd_inf("Default executor running callback, v[{}]", v);
        return v + 3;
    }).thenValue([](int v) {
        spd_inf("Default executor running second callback, v[{}]", v);
        return v + 4;
    });
}

void executor_example::base_example() {
    spd_inf("executor base example...");
    inline_executor_example();
    executor_future_example();
    drivable_executor_example();
    queued_immediate_executor_example();
}

/**
 * @brief inline executor example
 * @note
 * 优先使用 lambda（捕获尽量少、使用引用捕获当且仅当安全）。
 * 避免把大对象按值捕获到 lambda（若捕获大对象，考虑捕获指针/引用或 shared_ptr）。
 * 如果确实需要 std::bind（例如与现有 API 兼容），注意闭包尺寸：可在编译期用 sizeof 检查是否会超出 folly::Function 的内置缓冲区，从而触发堆分配。
 * 若极端性能敏感，避免类型擦除（即不要把可调用体包装成 folly::Function）；改用模板接口直接接受可调用（编译期多态，可内联调用）。
 * 但 Executor 接口定义为接收 folly::Function，这限制了这一替代方案，除非你改变接口或实现一个模板化的 fast-path。
 */
void executor_example::inline_executor_example() {
    folly::InlineExecutor ex;
    // 直接调用lambda包装带参数的接口的调用，add()会立即执行。
    ex.add([&]() {
        spd_inf("executor inner example...");
        c_func(32);
    });
    // bind 返回可调用对象，接受 void()
    ex.add(std::bind(&c_func, 10));
    // 直接传递C函数地址指针
    ex.add(&c_fn);
    // 可直接传递C++函数指针，可以隐式转为可调用对象
    ex.add(&foo);

    // 对象函数
    S s;
    ex.add([&s](){ s.m(7); });
    ex.add(std::bind(&S::m, &s, 8));

    auto ka = folly::getKeepAliveToken(&ex); // 产生命名 keep-alive
    // ka.add([](auto&& keep){...} 编译错误！
    // 原来的 ka 仍然可用
    ka.copy().add([](auto&& keep){
        spd_inf("keepAlive ex copy");
    });
    // ka 不可用
    std::move(ka).add([](auto&& keep){ 
        // 这里的可调用接收 ExecutorKeepAlive，确保 executor 在此期间仍然有效
        // 对 InlineExecutor 来说，这通常是 dummy keep-alive（无阻塞保障）
        // do work...
        spd_inf("keepAlive ex");
    });

    future_default_executor();
}

void executor_example::executor_future_example(){
    folly::InlineExecutor ex;
    folly::Promise<int> p;
    //SemiFuture：不含（或不固定）执行器；适合库/API 返回，允许调用者选择执行上下文（用 .via(...)）。
    //Future：通常已经设置了执行器（或约定的执行策略），续链会在该执行器上运行；更“完整/确定”。
    auto sf = p.getSemiFuture();

    // 直接 inline 执行回调
    auto f = std::move(sf).via(&ex).thenValue([](int v) {
        spd_inf("InlineExecutor running callback, v[{}]", v);
        return v + 1;
    });

    p.setValue(10);
    spd_inf("Result: {}", std::move(f).get());
}

static void manual_executor_example_basic(){
    folly::ManualExecutor ex;
    ex.add([](){
        spd_inf("manual executor basic example...");
    });
    ex.drive();
}

static void manual_executor_example_advanced(){
    folly::ManualExecutor ex; // 创建手动执行器
    
    folly::Promise<int> p; // 创建 Promise
    auto sf = p.getSemiFuture(); // 获取与 Promise 关联的 SemiFuture
    // 将回调排队到手动执行器上
    auto f = std::move(sf).via(&ex).thenValue([](int v) {
        spd_inf("ManualExecutor running callback, v[{}]", v);
        return v + 1;
    });
    // 设置 Promise 的值，触发回调排队
    p.setValue(20);

    // 创建另一个 Promise 和 SemiFuture，排队另一个回调
    folly::Promise<int> p1;
    auto sf1 = p1.getSemiFuture();
    auto f1 = std::move(sf1).via(&ex).thenValue([](int v) {
        spd_inf("ManualExecutor running callback, v[{}]", v);
        return v + 2;
    });
    p1.setValue(30);
    // 手动驱动执行器以运行排队的任务
    ex.drive();

    // 只调用一次 get()，把结果保存起来供后续使用
    try {
        int result = std::move(f).get();
        spd_inf("Result: {}", result);

        int result1 = std::move(f1).get();
        spd_inf("Result1: {}", result1);
    } catch (const std::exception& e) {
        spd_err("exception from get(): {}", e.what());
    } catch (...) {
        spd_err("unknown exception from get()");
    }
}

static void manual_executor_example(){
    manual_executor_example_basic();
    manual_executor_example_advanced();
}

static void base_event_example(){
    
}
void executor_example::drivable_executor_example(){
    manual_executor_example();
    base_event_example();
}

static void queued_immediate_executor_example_basic(){
    folly::QueuedImmediateExecutor& ex = folly::QueuedImmediateExecutor::instance();
    ex.add([](){
        spd_inf("queued immediate executor basic example...");
    });
}

static void nest_queued_immediate_executor_example(){
    spd_inf("nested inline executor example...");
    // Expected output order: 
    folly::InlineExecutor x = folly::InlineExecutor::instance();
    size_t counter = 0;
    x.add([&] {
        spd_inf("inline level 1: {}", ++counter);
        x.add([&] {
            spd_inf("inline level 2: {}", ++counter);
            x.add([&] {
                spd_inf("inline level 3: {}", ++counter);
            });
            spd_inf("inline level >2: {}", ++counter);
        });
        spd_inf("inline level >1: {}", ++counter);
    });
    spd_inf("inline level >0: {}", ++counter);

    spd_inf("nested queued immediate executor example...");
    counter = 0;
    folly::QueuedImmediateExecutor& ex = folly::QueuedImmediateExecutor::instance();
    ex.add([&ex, &counter](){
        spd_inf("queued level 1: {}", ++counter);
        ex.add([&ex, &counter](){
            spd_inf("queued level 2: {}", ++counter);
            ex.add([&counter](){
                spd_inf("queued level: 3 {}", ++counter);
            });
            spd_inf("queued level >2: {}", ++counter);
        });
        spd_inf("queued level >1: {}", ++counter);
    });
    spd_inf("queued level >0: {}", ++counter);
}
void executor_example::queued_immediate_executor_example(){
    queued_immediate_executor_example_basic();
    nest_queued_immediate_executor_example();  
    benchmark_queued_immediate_executor();
}

void executor_example::benchmark_queued_immediate_executor(){
    spd_inf("folly executor benchmark (QueuedImmediate vs SerialExecutor)");

    constexpr size_t kIterations = 100000; // 每种方案循环次数

    auto tinyWork = [](size_t& counter) {
        counter++;
    };

    // 使用固定规模的 CPU 线程池
    folly::CPUThreadPoolExecutor pool(4);

    // 方案一：CPUThreadPoolExecutor + QueuedImmediateExecutor
    {
        size_t counter = 0;
        stopwatch_tsc clock;

        std::thread::id workerThreadId;
        folly::Baton done;

        clock.start();
        pool.add([&] {
            workerThreadId = std::this_thread::get_id();
            for (size_t i = 0; i < kIterations; ++i) {
                folly::QueuedImmediateExecutor::instance().add([&] { tinyWork(counter); });
                folly::QueuedImmediateExecutor::instance().add([&] { tinyWork(counter); });
                folly::QueuedImmediateExecutor::instance().add([&] { tinyWork(counter); });
            }
            done.post();
        });

        done.wait();
        auto ms = clock.elapsed_ms();
        spd_inf("=== CPUThreadPoolExecutor + QueuedImmediateExecutor ===");
        spd_inf("counter={} (expect {})", counter, 3 * kIterations);
        spd_inf("elapsed={} ms", ms);
    }

    // 方案二：CPUThreadPoolExecutor + SerialExecutor
    {
        size_t counter = 0;
        auto serial = folly::SerialExecutor::create(&pool);

        stopwatch_tsc clock;

        folly::Baton done;

        clock.start();
        for (size_t i = 0; i < kIterations; ++i) {
            serial->add([&] { tinyWork(counter); });
            serial->add([&] { tinyWork(counter); });
            serial->add([&] {
                tinyWork(counter);
                if (counter == 3 * kIterations) {
                    done.post();
                }
            });
        }

        done.wait();

        auto ms = clock.elapsed_ms();
        spd_inf("=== CPUThreadPoolExecutor + SerialExecutor ===");
        spd_inf("counter={} (expect {})", counter, 3 * kIterations);
        spd_inf("elapsed={} ms", ms);
    }
}