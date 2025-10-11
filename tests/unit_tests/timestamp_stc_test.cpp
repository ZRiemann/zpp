// Unit tests for the TSC-based stopwatch adapter
#include <gtest/gtest.h>

#include <zpp/system/stopwatch/stopwatch_tsc.h>
#include <zpp/system/tsc.h>
#include <thread>
#include <chrono>

namespace {

class StopwatchTscTest : public ::testing::Test {
protected:
	void SetUp() override {
		z::tsc_init();
	}
	void TearDown() override {
	}
};

TEST_F(StopwatchTscTest, BasicElapsedIncreases) {
	z::stopwatch_tsc sw;
	sw.start();
	// Wait until elapsed_ns reports a non-zero value (busy loop, short)
	constexpr int kIterations = 1000;
	bool observed = false;
	for (int i = 0; i < kIterations; ++i) {
		auto ns = sw.elapsed_ns();
		if (ns > 0) { observed = true; break; }
	}
	EXPECT_TRUE(observed) << "stopwatch_tsc should report increasing elapsed time";
}

TEST_F(StopwatchTscTest, StepUpdatesBaseline) {
	z::stopwatch_tsc sw;
	sw.start();
	auto d1 = sw.step_ns();
	// do a short pause to consume some measurable time (avoid deprecated volatile usage)
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	auto d2 = sw.step_ns();
	// second step should measure some time (>= 0) and probably differ from first
	EXPECT_GE(d1, 1);
    EXPECT_LE(d1, 50);
	EXPECT_GE(d2, 1000000);
    EXPECT_LE(d2, 1090000);
}

} // namespace

