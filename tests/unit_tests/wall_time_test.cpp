#include <gtest/gtest.h>

#include <zpp/system/wall_time.hpp>

#include <chrono>
#include <regex>
#include <string>

namespace {

TEST(WallTimeTest, EpochMillisecondsTracksSystemClock) {
    const auto before = std::chrono::duration_cast<std::chrono::milliseconds>(
        z::wall_time::clock_type::now().time_since_epoch()).count();
    const auto epoch = z::wall_time::epoch_ms();
    const auto after = std::chrono::duration_cast<std::chrono::milliseconds>(
        z::wall_time::clock_type::now().time_since_epoch()).count();

    EXPECT_GE(epoch, before);
    EXPECT_LE(epoch, after);
}

TEST(WallTimeTest, FormatsTimePointWithSixMicrosecondDigits) {
    std::string out;
    z::wall_time::to_str(z::wall_time::clock_type::now(), out);

    EXPECT_EQ(out.size(), 26U);
    EXPECT_TRUE(std::regex_match(out, std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6})")));
}

TEST(WallTimeTest, NowStrReusesOutputString) {
    std::string out = "existing-buffer";
    std::string& returned = z::wall_time::now_str(out);

    EXPECT_EQ(&returned, &out);
    EXPECT_EQ(out.size(), 26U);
    EXPECT_TRUE(std::regex_match(out, std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6})")));
}

TEST(WallTimeTest, EpochMicrosecondsIsAtLeastEpochMilliseconds) {
    const auto epoch_ms = z::wall_time::epoch_ms();
    const auto epoch_us = z::wall_time::epoch_us();

    EXPECT_GE(epoch_us, epoch_ms * 1000);
}

} // namespace
