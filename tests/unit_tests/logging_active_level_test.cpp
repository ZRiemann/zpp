#include <gtest/gtest.h>

#define SPDLOG_ACTIVE_LEVEL 2
#include <zpp/spdlog.h>

TEST(LoggingActiveLevelTest, DebugMacroIsCompileTimeDisabled) {
  int side_effects = 0;
  spd_dbg("debug {}", ++side_effects);
  EXPECT_EQ(side_effects, 0);
}

TEST(LoggingActiveLevelTest, InfoMacroStillRunsWhenActive) {
  z::log::config cfg;
  cfg.console = false;
  cfg.rotating_file = false;
  cfg.level = spdlog::level::info;
  cfg.flush_interval = std::chrono::seconds(0);

  z::log::init(cfg);
  int side_effects = 0;
  spd_inf("info {}", ++side_effects);
  z::log::shutdown();

  EXPECT_EQ(side_effects, 1);
}
