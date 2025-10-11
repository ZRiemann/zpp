// Unit tests for the chrono-based stopwatch adapter
#include <gtest/gtest.h>

#include <zpp/system/stopwatch/stopwatch_chrono.h>
#include <chrono>
#include <thread>

namespace {

class StopwatchChronoTest : public ::testing::Test {
protected:
	void SetUp() override {}
	void TearDown() override {}
};

TEST_F(StopwatchChronoTest, BasicElapsedIncreases) {
	z::stopwatch_chrono sw;
	sw.start();
	constexpr int kIterations = 1000;
	bool observed = false;
	for (int i = 0; i < kIterations; ++i) {
		auto ns = sw.elapsed_ns();
		if (ns > 0) { observed = true; break; }
	}
	EXPECT_TRUE(observed) << "chrono stopwatch should report increasing elapsed time";
}

TEST_F(StopwatchChronoTest, StepUpdatesBaseline) {
	z::stopwatch_chrono sw;
	sw.start();
	auto d1 = sw.step_ns();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	auto d2 = sw.step_ns();
	EXPECT_GE(d1, 1);
    EXPECT_GE(d2, 1000000);
    EXPECT_LE(d2, 1090000);
}

} // namespace

