#include <gtest/gtest.h>
#include <zpp/core/monitor.h>
#include <zpp/system/stopwatch/stopwatch_tsc.h>
#include <glog/logging.h>
#include <zpp/spdlog.h>

namespace {

class BenchmarkCPUExecutor : public ::testing::Test {
public:
    BenchmarkCPUExecutor() = default;

    ~BenchmarkCPUExecutor() override = default;
protected:
    void SetUp() override {
        z::monitor_guard mon_guard;
        z::tsc_init();
    }

    void TearDown() override {
        //z::monitor_guard::print_statistic();
    }

};

TEST_F(BenchmarkCPUExecutor, BasicTest) {
    z::monitor_guard mon_guard;
    z::stopwatch_tsc sw;
    sw.start();
    for (int i = 0; i < 10; ++i) {
        // Busy loop
        spd_inf("Iteration {}", i);
    }
    auto elapsed = sw.elapsed_ns();
    spd_inf("Elapsed time (ns): {}", elapsed);
    EXPECT_TRUE(true);
}

}