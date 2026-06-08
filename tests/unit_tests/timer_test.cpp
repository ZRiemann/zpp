#include <gtest/gtest.h>

#include <zpp/system/timer.hpp>

#include <chrono>

namespace {

TEST(TimerTest, ElapsedSecondsUsesStoredBaseline)
{
    const auto baseline = z::timer<>::clock_type::now() - std::chrono::seconds(1);
    const z::timer<> timer(baseline);

    EXPECT_GE(timer.elapsed_sec(), 1);
}

TEST(TimerTest, UpdateResetsBaseline)
{
    z::timer<> timer(z::timer<>::clock_type::now() - std::chrono::milliseconds(10));
    EXPECT_GE(timer.elapsed_ms(), 10);

    const auto last_time = timer.last_time();
    timer.update();

    EXPECT_GE(timer.last_time(), last_time);
    EXPECT_LT(timer.elapsed_ms(), 1000);
}

} // namespace
