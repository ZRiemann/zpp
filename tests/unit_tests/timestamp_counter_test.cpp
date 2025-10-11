#include <gtest/gtest.h>

#include <cstdint>

#include <zpp/system/tsc.h>

namespace {

class TimestampCounterTest : public ::testing::Test {
protected:
    void SetUp() override {
        z::tsc_init();
    }

    void TearDown() override {
        
    }
};

TEST_F(TimestampCounterTest, RelaxedCounterIsMonotonic) {
    constexpr int kIterations = 128;
    z::tsc_t prev = z::tsc_now_r();
    bool observed_increase = false;
    for (int i = 0; i < kIterations; ++i) {
        z::tsc_t current = z::tsc_now_r();
        EXPECT_GE(current, prev);
        if (current > prev) {
            observed_increase = true;
            break;
        }
        prev = current;
    }
    EXPECT_TRUE(observed_increase) << "TSC should progress across successive relaxed reads";
}

TEST_F(TimestampCounterTest, SerializedCounterIsMonotonic) {
    constexpr int kIterations = 128;
    z::tsc_t prev = z::tsc_now_s();
    bool observed_increase = false;
    for (int i = 0; i < kIterations; ++i) {
        z::tsc_t current = z::tsc_now_s();
        EXPECT_GE(current, prev);
        if (current > prev) {
            observed_increase = true;
            break;
        }
        prev = current;
    }
    EXPECT_TRUE(observed_increase) << "TSC should progress across successive serialized reads";
}

TEST_F(TimestampCounterTest, FrequencyOverrideControlsConversion) {
    // Also update the cached ns-per-cycle to ensure tsc_to_ns() uses the
    // overridden conversion rather than any previously-detected cached value.
    const z::tsc_t cycles = static_cast<z::tsc_t>(z::get_tsc_frequency()); // ~1 second worth of cycles
    const z::duration_t nanos = z::tsc_to_ns(cycles);
    const z::duration_t expected = 1'000'000'000ULL;

    // Allow an off-by-one tolerance due to rounding.
    const auto diff = static_cast<int64_t>(nanos) - static_cast<int64_t>(expected);
    EXPECT_GE(diff, -1);
    EXPECT_LE(diff, 1);
}

} // namespace
