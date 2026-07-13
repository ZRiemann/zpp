#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <rapidjson/document.h>
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

std::vector<std::string> read_lines(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
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

void expect_json_string(const rapidjson::Document &doc, const char *name,
                        const char *value) {
  ASSERT_TRUE(doc.HasMember(name)) << name;
  ASSERT_TRUE(doc[name].IsString()) << name;
  EXPECT_STREQ(doc[name].GetString(), value);
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

TEST(LoggingTest, ImportantJsonlSinkKeepsWarnAndErrorRecords) {
  const auto runtime_path =
      std::filesystem::current_path() / "zpp_logging_runtime_sink_test.log";
  const auto important_path =
      std::filesystem::current_path() / "zpp_logging_important_sink_test.jsonl";
  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
  auto cfg = file_config(runtime_path);
  cfg.service = "zpp-test";
  cfg.node = "node-a";
  cfg.run_id = "run-1";
  cfg.important_file.enabled = true;
  cfg.important_file.file_name = important_path.string();
  cfg.important_file.max_file_size_mb = 1;
  cfg.important_file.max_files = 1;
  cfg.important_file.level = spdlog::level::warn;
  cfg.important_file.format = z::log::sink_format::jsonl;

  z::log::init(cfg);
  spd_inf("[ctp-md:depth] ignored info");
  spd_war("[ctp-md:login-rsp] warning {}", 7);
  spd_err("[nng:socket:create] failed {}", 9);
  z::log::shutdown();

  const auto runtime_content = read_file(runtime_path);
  EXPECT_NE(runtime_content.find("I [ctp-md:depth] ignored info"),
            std::string::npos);

  const auto lines = read_lines(important_path);
  ASSERT_EQ(lines.size(), 2);

  rapidjson::Document warn_doc;
  ASSERT_FALSE(warn_doc.Parse(lines[0].c_str()).HasParseError());
  expect_json_string(warn_doc, "level", "warn");
  expect_json_string(warn_doc, "service", "zpp-test");
  expect_json_string(warn_doc, "node", "node-a");
  expect_json_string(warn_doc, "run_id", "run-1");
  expect_json_string(warn_doc, "component", "ctp-md");
  expect_json_string(warn_doc, "action", "login-rsp");
  ASSERT_TRUE(warn_doc.HasMember("file"));
  ASSERT_TRUE(warn_doc.HasMember("line"));
  ASSERT_TRUE(warn_doc.HasMember("function"));
  ASSERT_TRUE(warn_doc.HasMember("tid"));

  rapidjson::Document err_doc;
  ASSERT_FALSE(err_doc.Parse(lines[1].c_str()).HasParseError());
  expect_json_string(err_doc, "level", "error");
  expect_json_string(err_doc, "component", "nng.socket");
  expect_json_string(err_doc, "action", "create");
  expect_json_string(err_doc, "message", "[nng:socket:create] failed 9");

  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
}

TEST(LoggingTest, ImportantMacroWritesInfoOnlyToImportantChannel) {
  const auto runtime_path =
      std::filesystem::current_path() / "zpp_logging_imp_runtime_test.log";
  const auto important_path =
      std::filesystem::current_path() / "zpp_logging_imp_important_test.jsonl";
  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
  auto cfg = file_config(runtime_path);
  cfg.service = "zpp-test";
  cfg.important_file.enabled = true;
  cfg.important_file.file_name = important_path.string();
  cfg.important_file.max_file_size_mb = 1;
  cfg.important_file.max_files = 1;
  cfg.important_file.level = spdlog::level::warn;
  cfg.important_file.format = z::log::sink_format::jsonl;

  z::log::init(cfg);
  spd_inf("[system:heartbeat] ordinary info");
  spd_imp("[system:startup] service started {}", 1);
  spd_war("[system:shutdown] service stopped");
  z::log::shutdown();

  const auto runtime_content = read_file(runtime_path);
  EXPECT_NE(runtime_content.find("I [system:heartbeat] ordinary info"),
            std::string::npos);
  EXPECT_NE(runtime_content.find("I [system:startup] service started 1"),
            std::string::npos);

  const auto lines = read_lines(important_path);
  ASSERT_EQ(lines.size(), 2);

  rapidjson::Document imp_doc;
  ASSERT_FALSE(imp_doc.Parse(lines[0].c_str()).HasParseError());
  expect_json_string(imp_doc, "level", "info");
  expect_json_string(imp_doc, "component", "system");
  expect_json_string(imp_doc, "action", "startup");
  expect_json_string(imp_doc, "message", "[system:startup] service started 1");
  EXPECT_FALSE(imp_doc.HasMember("important"));

  rapidjson::Document warn_doc;
  ASSERT_FALSE(warn_doc.Parse(lines[1].c_str()).HasParseError());
  expect_json_string(warn_doc, "level", "warn");
  expect_json_string(warn_doc, "component", "system");
  expect_json_string(warn_doc, "action", "shutdown");

  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
}

TEST(LoggingTest, ImportantMacroUsesIndependentChannelWhenRuntimeInfoDisabled) {
  const auto runtime_path =
      std::filesystem::current_path() / "zpp_logging_imp_level_runtime.log";
  const auto important_path =
      std::filesystem::current_path() / "zpp_logging_imp_level_important.jsonl";
  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
  auto cfg = file_config(runtime_path);
  cfg.level = spdlog::level::warn;
  cfg.important_file.enabled = true;
  cfg.important_file.file_name = important_path.string();
  cfg.important_file.max_file_size_mb = 1;
  cfg.important_file.max_files = 1;
  cfg.important_file.level = spdlog::level::warn;
  cfg.important_file.format = z::log::sink_format::jsonl;

  z::log::init(cfg);
  int side_effects = 0;
  spd_inf("[system:heartbeat] ordinary info {}", ++side_effects);
  EXPECT_EQ(side_effects, 0);
  spd_imp("[system:startup] service started {}", ++side_effects);
  EXPECT_EQ(side_effects, 1);
  z::log::shutdown();

  const auto runtime_content = read_file(runtime_path);
  EXPECT_EQ(runtime_content.find("service started 1"), std::string::npos);

  const auto lines = read_lines(important_path);
  ASSERT_EQ(lines.size(), 1);
  rapidjson::Document doc;
  ASSERT_FALSE(doc.Parse(lines[0].c_str()).HasParseError());
  expect_json_string(doc, "level", "info");
  expect_json_string(doc, "message", "[system:startup] service started 1");

  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
}

TEST(LoggingTest, AsyncImportantJsonlPreservesThreadIdAndQueueOrder) {
  const auto runtime_path =
      std::filesystem::current_path() / "zpp_logging_async_runtime_test.log";
  const auto important_path = std::filesystem::current_path() /
                              "zpp_logging_async_important_test.jsonl";
  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
  auto cfg = file_config(runtime_path);
  cfg.async = true;
  cfg.important_file.enabled = true;
  cfg.important_file.file_name = important_path.string();
  cfg.important_file.max_file_size_mb = 1;
  cfg.important_file.max_files = 1;
  cfg.important_file.level = spdlog::level::warn;
  cfg.important_file.format = z::log::sink_format::jsonl;

  int worker_tid = -1;
  z::log::init(cfg);
  std::thread worker([&] {
    worker_tid = z::tid::id();
    spd_war("[async-test:worker-warn] payload");
    spd_imp("[async-test:worker-important] payload");
  });
  worker.join();
  z::log::shutdown();

  const auto lines = read_lines(important_path);
  ASSERT_EQ(lines.size(), 2);
  rapidjson::Document warn_doc;
  ASSERT_FALSE(warn_doc.Parse(lines[0].c_str()).HasParseError());
  expect_json_string(warn_doc, "level", "warn");
  expect_json_string(warn_doc, "action", "worker-warn");
  ASSERT_TRUE(warn_doc.HasMember("tid"));
  ASSERT_TRUE(warn_doc["tid"].IsUint64());
  EXPECT_EQ(warn_doc["tid"].GetUint64(),
            static_cast<std::uint64_t>(worker_tid));

  rapidjson::Document important_doc;
  ASSERT_FALSE(important_doc.Parse(lines[1].c_str()).HasParseError());
  expect_json_string(important_doc, "level", "info");
  expect_json_string(important_doc, "action", "worker-important");
  ASSERT_TRUE(important_doc.HasMember("tid"));
  ASSERT_TRUE(important_doc["tid"].IsUint64());
  EXPECT_EQ(important_doc["tid"].GetUint64(),
            static_cast<std::uint64_t>(worker_tid));

  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
}
