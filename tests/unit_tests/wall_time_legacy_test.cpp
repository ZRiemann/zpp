#include <gtest/gtest.h>

#include <zpp/system/wall_time.hpp>

#include <chrono>
#include <regex>
#include <string>

namespace {

TEST(WallTimeLegacyTest, FormatsTimePointWithSixMicrosecondDigits)
{
    std::string out;
    z::wall_time::to_str(z::wall_time::clock_type::now(), out);

    EXPECT_EQ(out.size(), 26U);
    EXPECT_TRUE(std::regex_match(out, std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6})")));
}

TEST(WallTimeLegacyTest, PreservesFixedMicrosecondPrecision)
{
    const auto tp = z::wall_time::time_point{} + std::chrono::seconds(1) + std::chrono::microseconds(123456);

    std::string out;
    z::wall_time::to_str(tp, out);

    EXPECT_EQ(out.size(), 26U);
    EXPECT_EQ(out.substr(20), "123456");
}

TEST(WallTimeLegacyTest, NowStrReusesOutputString)
{
    std::string out = "existing-buffer";
    std::string& returned = z::wall_time::now_str(out);

    EXPECT_EQ(&returned, &out);
    EXPECT_EQ(out.size(), 26U);
    EXPECT_TRUE(std::regex_match(out, std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6})")));
}

} // namespace
