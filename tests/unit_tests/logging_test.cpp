#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <zpp/spd_log.h>
#include <zpp/spdlog.h>

namespace {

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

z::log::config file_config(const std::filesystem::path& path)
{
    z::log::config cfg;
    cfg.console = false;
    cfg.rotating_file = true;
    cfg.rotate_on_open = true;
    cfg.file_name = path.string();
    cfg.max_file_size_mb = 1;
    cfg.max_files = 1;
    cfg.pattern = "%L %v";
    cfg.flush_interval = std::chrono::seconds(0);
    return cfg;
}

} // namespace

TEST(LoggingTest, FallbackLoggerIsSafeBeforeInit)
{
    z::log::shutdown();
    spd_inf("this is discarded by the fallback logger {}", 1);
    z::log::shutdown();
}

TEST(LoggingTest, ShutdownIsIdempotent)
{
    z::log::shutdown();
    z::log::shutdown();
}

TEST(LoggingTest, PrintfWrapperMapsWarnLevel)
{
    const auto path = std::filesystem::current_path() / "zpp_logging_warn_test.log";
    std::filesystem::remove(path);

    z::log::init(file_config(path));
    spd2war("warning %d", 7);
    z::log::shutdown();

    const auto content = read_file(path);
    EXPECT_NE(content.find("W warning 7"), std::string::npos);
    std::filesystem::remove(path);
}

TEST(LoggingTest, PrintfWrapperTruncatesLongMessagesSafely)
{
    const auto path = std::filesystem::current_path() / "zpp_logging_long_test.log";
    std::filesystem::remove(path);
    const std::string long_message(6000, 'x');

    z::log::init(file_config(path));
    spd2inf("%s", long_message.c_str());
    z::log::shutdown();

    const auto content = read_file(path);
    EXPECT_NE(content.find("I "), std::string::npos);
    EXPECT_LT(content.size(), long_message.size());
    std::filesystem::remove(path);
}
