#include <gtest/gtest.h>
#include <zpp/core/monitor.h>
#include <zpp/system/stopwatch/stopwatch_tsc.h>
#include <glog/logging.h>
#include <zpp/spdlog.h>
#include <folly/coro/Task.h>
#include <folly/coro/BlockingWait.h>
#include <folly/Try.h>
#include <zpp/folly/coro.h>

namespace {
using folly::coro::blockingWait;
using folly::coro::Task;
using mon_guard = z::monitor_guard;

class FollyCoroTest : public ::testing::Test {
public:
    FollyCoroTest() = default;

    ~FollyCoroTest() override = default;
protected:
    void SetUp() override {
    }

    void TearDown() override {
        mon_guard::print_statistic();
    }

    Task<int> sample_folly_task() {
        spd_inf("Inside sample_folly_task");
        mon_guard guard;
        co_return 42;
    }

    Task<void> will_throw(){
        mon_guard guard;
        throw std::runtime_error("Intentional error");
        co_return;
    }
#if 0
    Task<void> consumer_try(){
        mon_guard guard;
        auto t = sample_folly_task();
        folly::Try<int> result = co_await t;
        if (result.hasException()) {
            spd_err("Task failed with exception: {}", result.exception().what());
        } else {
            spd_inf("Task completed with result: {}", result.value());
        }

        folly::Try<void> t1 = co_await folly::coro::co_awaitTry(will_throw());
        if (t1.hasException()) {
            spd_inf("Caught expected exception: {}", t1.exception().what());
        } else {
            spd_err("Expected exception was not thrown");
        }
        co_return;
    }
#endif
    Task<int> cpu_work(int n){
        mon_guard guard;
        spd_inf("Starting CPU work for n={}", n);
        int sum = 0;
        for(int i = 0; i < n; ++i){
            sum += i;
        }
        spd_inf("Completed CPU work for sum={}", sum);
        co_return sum;
    }

    Task<void> blocking_io_work(){
        mon_guard guard;
        // 模拟阻塞IO操作
        spd_inf("Begin blocking IO work");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        spd_inf("Completed blocking IO work");
        co_return;
    }

    // Compose API: AsyncGenerator, Stream, Collect/Merge/Transform
    /*
    在 Folly 协程/异步流领域，“组合子与并发聚合”常用的英文词汇有：
    组合子：combinator（复数：combinators）
    例如：transform combinator, filter combinator, merge combinator
    并发聚合：concurrent aggregation 或 concurrent collection
    也可称为：concurrent gather, parallel aggregation, concurrent result aggregation
    相关 API/术语：
    merge（合并/归并流）
    collectAll（批量收集/聚合）
    collectAllWindow（窗口并发聚合，windowed concurrent aggregation）
    transform（变换/映射，transformation combinator）
    pipeline（流水线/管道，data pipeline）
    fan-in（多路归并，fan-in pattern）
    fan-out（多路分发，fan-out pattern）
    */
};

TEST_F(FollyCoroTest, BasicTest) {
    auto t = sample_folly_task();

    // 手动执行并等待协程完成（将所有权移动给 blockingWait）
    int v = folly::coro::blockingWait(std::move(t));
    EXPECT_EQ(v, 42);
}

TEST_F(FollyCoroTest, ZFoCoBasicTest) {
    auto t = z::fo::co_cpu(sample_folly_task());

    // 手动执行并等待协程完成（将所有权移动给 blockingWait）
    int v = z::fo::co_blocking_wait(std::move(z::fo::co_cpu(sample_folly_task())));
    EXPECT_EQ(v, 42);
}

TEST_F(FollyCoroTest, ThrowTest) {
    try{
        auto t = will_throw();
        folly::coro::blockingWait(std::move(t));
        FAIL() << "Expected std::runtime_error";
    }catch(const std::runtime_error& e){
        EXPECT_STREQ(e.what(), "Intentional error");
    }catch(...){
        FAIL() << "Expected std::runtime_error";
    }
}

TEST_F(FollyCoroTest, TryTest) {
    auto coro = [this]() -> folly::coro::Task<folly::Try<void>> {
        auto r = co_await folly::coro::co_awaitTry(will_throw());
        co_return r;
    };

    folly::Try<void> t = folly::coro::blockingWait(std::move(coro()));
    if (t.hasException()) {
        spd_inf("Caught expected exception: {}", t.exception().what());
    } else {
        FAIL() << "Expected exception was not thrown";
    }
}

TEST_F(FollyCoroTest, IoWorkTest) {
    // 等待阻塞IO协程完成
    auto ioTaskWithExec = folly::coro::co_withExecutor(folly::getGlobalIOExecutor(), blocking_io_work());
    // 如果到达这里，说明协程已成功完成
    blockingWait(std::move(ioTaskWithExec));

    SUCCEED();
}

TEST_F(FollyCoroTest, CPUWorkTest) {
    //auto cpuTaskWithExec = folly::coro::co_withExecutor(folly::getGlobalCPUExecutorWeakRef(), cpu_work(1000000));
    auto cpuTaskWithExec = folly::coro::co_withExecutor(folly::getGlobalCPUExecutor(), cpu_work(1000000));
    int result = folly::coro::blockingWait(std::move(cpuTaskWithExec));
    //EXPECT_EQ(result, 499500); // 0 + 1 + ... + 999
    spd_inf("CPU work result: {}", result);
    SUCCEED();
}

// AsyncScope
// CancellableAsyncScope
// Typical usage: add pipeline jobs with scope.add(...); 
// on shutdown call scope.requestCancellation() (optional) 
// → co_await scope.joinAsync() or call cancelAndJoinAsync().
} // namespace
