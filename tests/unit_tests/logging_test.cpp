#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <zpp/spd_log.h>
#include <zpp/spdlog.h>
#include <zpp/system/tid.h>

namespace {

std::string read_file(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

z::log::config file_config(const std::filesystem::path &path) {
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

std::string short_tid(int tid) {
  const auto text = std::to_string(tid);
  return text.size() == 1 ? "0" + text : text;
}

} // namespace

TEST(LoggingTest, FallbackLoggerIsSafeBeforeInit) {
  z::log::shutdown();
  spd_inf("this is discarded by the fallback logger {}", 1);
  z::log::shutdown();
}

TEST(LoggingTest, ShutdownIsIdempotent) {
  z::log::shutdown();
  z::log::shutdown();
}

TEST(LoggingTest, PrintfWrapperMapsWarnLevel) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_warn_test.log";
  std::filesystem::remove(path);

  z::log::init(file_config(path));
  spd2war("warning %d", 7);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find("W warning 7"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, RuntimeLevelShortCircuitsDisabledArguments) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_runtime_level_test.log";
  std::filesystem::remove(path);
  auto cfg = file_config(path);
  cfg.level = spdlog::level::warn;

  z::log::init(cfg);
  int side_effects = 0;
  spd_dbg("debug {}", ++side_effects);
  EXPECT_EQ(side_effects, 0);
  spd_war("warn {}", ++side_effects);
  EXPECT_EQ(side_effects, 1);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_EQ(content.find("debug"), std::string::npos);
  EXPECT_NE(content.find("W warn 1"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, PatternThreadIdUsesZppShortThreadId) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_short_tid_test.log";
  std::filesystem::remove(path);
  auto cfg = file_config(path);
  cfg.pattern = "%t %v";

  const int main_tid = z::tid::id();
  int worker_tid = -1;

  z::log::init(cfg);
  spd_inf("main");
  std::thread worker([&] {
    worker_tid = z::tid::id();
    spd_inf("worker");
  });
  worker.join();
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find(short_tid(main_tid) + " main"), std::string::npos);
  EXPECT_NE(content.find(short_tid(worker_tid) + " worker"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, AsyncPatternThreadIdUsesCallingThreadId) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_async_short_tid_test.log";
  std::filesystem::remove(path);
  auto cfg = file_config(path);
  cfg.async = true;
  cfg.pattern = "%t %v";

  int worker_tid = -1;

  z::log::init(cfg);
  std::thread worker([&] {
    worker_tid = z::tid::id();
    spd_inf("worker_async");
  });
  worker.join();
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find(short_tid(worker_tid) + " worker_async"),
            std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, PrintfWrapperPatternThreadIdUsesZppShortThreadId) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_printf_short_tid_test.log";
  std::filesystem::remove(path);
  auto cfg = file_config(path);
  cfg.pattern = "%t %v";

  const int main_tid = z::tid::id();

  z::log::init(cfg);
  spd2war("printf %d", 7);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find(short_tid(main_tid) + " printf 7"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, RuntimeShortCircuitKeepsBacktraceMessages) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_backtrace_test.log";
  std::filesystem::remove(path);
  auto cfg = file_config(path);
  cfg.level = spdlog::level::err;

  z::log::init(cfg);
  spd_begin(4);
  int side_effects = 0;
  spd_dbg("debug {}", ++side_effects);
  EXPECT_EQ(side_effects, 1);
  spd_end();
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find("D debug 1"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, PrintfWrapperShortCircuitsDisabledArguments) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_printf_level_test.log";
  std::filesystem::remove(path);
  auto cfg = file_config(path);
  cfg.level = spdlog::level::warn;

  z::log::init(cfg);
  int side_effects = 0;
  spd2dbg("debug %d", ++side_effects);
  EXPECT_EQ(side_effects, 0);
  spd2war("warn %d", ++side_effects);
  EXPECT_EQ(side_effects, 1);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_EQ(content.find("debug"), std::string::npos);
  EXPECT_NE(content.find("W warn 1"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(LoggingTest, PrintfWrapperTruncatesLongMessagesSafely) {
  const auto path =
      std::filesystem::current_path() / "zpp_logging_long_test.log";
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
