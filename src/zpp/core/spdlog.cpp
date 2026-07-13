/**
 * @file spdlog.cpp
 * @brief Native-spdlog implementation for zpp logging initialization.
 */
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <zpp/spd_log.h>
#include <zpp/spdlog.h>
#include <zpp/system/tid.h>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/cfg/env.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/details/os.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

std::shared_ptr<spdlog::details::thread_pool> g_spd_pool;
constexpr spdlog::string_view_t zpp_tid_prefix{"\x1ezpp_tid:", 9};
constexpr char zpp_tid_suffix = '\x1f';

bool read_zpp_tid_payload(spdlog::string_view_t payload, std::size_t &thread_id,
                          spdlog::string_view_t &message) {
  if (payload.size() <= zpp_tid_prefix.size() ||
      std::char_traits<char>::compare(payload.data(), zpp_tid_prefix.data(),
                                      zpp_tid_prefix.size()) != 0) {
    return false;
  }

  std::size_t pos = zpp_tid_prefix.size();
  std::size_t id = 0;
  bool has_digit = false;
  while (pos < payload.size()) {
    const char ch = payload.data()[pos];
    if (ch < '0' || ch > '9') {
      break;
    }
    has_digit = true;
    id = id * 10 + static_cast<std::size_t>(ch - '0');
    ++pos;
  }

  if (!has_digit || pos >= payload.size() ||
      payload.data()[pos] != zpp_tid_suffix) {
    return false;
  }

  thread_id = id;
  message =
      spdlog::string_view_t{payload.data() + pos + 1, payload.size() - pos - 1};
  return true;
}

void append_spaces(std::size_t count, spdlog::memory_buf_t &dest) {
  for (std::size_t i = 0; i < count; ++i) {
    dest.push_back(' ');
  }
}

void append_short_thread_id(std::size_t thread_id, spdlog::memory_buf_t &dest) {
  if (thread_id < 10) {
    dest.push_back('0');
  }
  spdlog::details::fmt_helper::append_int(thread_id, dest);
}

spdlog::string_view_t level_name(spdlog::level::level_enum level) noexcept {
  switch (level) {
  case spdlog::level::trace:
    return spdlog::string_view_t{"trace", 5};
  case spdlog::level::debug:
    return spdlog::string_view_t{"debug", 5};
  case spdlog::level::info:
    return spdlog::string_view_t{"info", 4};
  case spdlog::level::warn:
    return spdlog::string_view_t{"warn", 4};
  case spdlog::level::err:
    return spdlog::string_view_t{"error", 5};
  case spdlog::level::critical:
    return spdlog::string_view_t{"critical", 8};
  case spdlog::level::off:
    return spdlog::string_view_t{"off", 3};
  default:
    return spdlog::string_view_t{"unknown", 7};
  }
}

void append_json_escaped(spdlog::string_view_t text,
                         spdlog::memory_buf_t &dest) {
  static constexpr char hex[] = "0123456789abcdef";
  for (char raw : text) {
    const auto ch = static_cast<unsigned char>(raw);
    switch (ch) {
    case '"':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\\"", 2}, dest);
      break;
    case '\\':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\\\", 2}, dest);
      break;
    case '\b':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\b", 2}, dest);
      break;
    case '\f':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\f", 2}, dest);
      break;
    case '\n':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\n", 2}, dest);
      break;
    case '\r':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\r", 2}, dest);
      break;
    case '\t':
      spdlog::details::fmt_helper::append_string_view(
          spdlog::string_view_t{"\\t", 2}, dest);
      break;
    default:
      if (ch < 0x20) {
        spdlog::details::fmt_helper::append_string_view(
            spdlog::string_view_t{"\\u00", 4}, dest);
        dest.push_back(hex[ch >> 4]);
        dest.push_back(hex[ch & 0x0f]);
      } else {
        dest.push_back(raw);
      }
      break;
    }
  }
}

void append_json_field(const char *name, spdlog::string_view_t value,
                       spdlog::memory_buf_t &dest, bool &first) {
  if (!first) {
    dest.push_back(',');
  }
  first = false;
  dest.push_back('"');
  spdlog::details::fmt_helper::append_string_view(
      spdlog::string_view_t{name, std::char_traits<char>::length(name)}, dest);
  spdlog::details::fmt_helper::append_string_view(
      spdlog::string_view_t{"\":\"", 3}, dest);
  append_json_escaped(value, dest);
  dest.push_back('"');
}

void append_json_field(const char *name, const std::string &value,
                       spdlog::memory_buf_t &dest, bool &first) {
  append_json_field(name, spdlog::string_view_t{value.data(), value.size()},
                    dest, first);
}

void append_json_field(const char *name, const char *value,
                       spdlog::memory_buf_t &dest, bool &first) {
  const auto *text = value == nullptr ? "" : value;
  append_json_field(
      name, spdlog::string_view_t{text, std::char_traits<char>::length(text)},
      dest, first);
}

void append_json_number_field(const char *name, std::size_t value,
                              spdlog::memory_buf_t &dest, bool &first) {
  if (!first) {
    dest.push_back(',');
  }
  first = false;
  dest.push_back('"');
  spdlog::details::fmt_helper::append_string_view(
      spdlog::string_view_t{name, std::char_traits<char>::length(name)}, dest);
  spdlog::details::fmt_helper::append_string_view(
      spdlog::string_view_t{"\":", 2}, dest);
  spdlog::details::fmt_helper::append_int(value, dest);
}

std::string make_iso_timestamp(spdlog::log_clock::time_point tp) {
  const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
  const auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(tp - seconds)
          .count();
  const std::time_t time = std::chrono::system_clock::to_time_t(tp);
  const std::tm tm = spdlog::details::os::localtime(time);
#ifdef SPDLOG_USE_STD_FORMAT
  return fmt_lib::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:06d}",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min, tm.tm_sec,
                         static_cast<int>(micros));
#else
  return fmt::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:06d}",
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                     tm.tm_min, tm.tm_sec, static_cast<int>(micros));
#endif
}

struct log_tags {
  std::string component;
  std::string action;
};

log_tags parse_log_tags(spdlog::string_view_t message) {
  log_tags tags;
  if (message.size() < 4 || message.data()[0] != '[') {
    return tags;
  }

  std::size_t closing = 1;
  while (closing < message.size() && message.data()[closing] != ']') {
    if (closing > 128) {
      return tags;
    }
    ++closing;
  }
  if (closing >= message.size() || closing <= 1) {
    return tags;
  }

  const std::string_view tag{message.data() + 1, closing - 1};
  const auto first_colon = tag.find(':');
  if (first_colon == std::string_view::npos || first_colon == 0 ||
      first_colon + 1 >= tag.size()) {
    return tags;
  }

  if (tag.substr(0, first_colon) == "nng") {
    const auto second_colon = tag.find(':', first_colon + 1);
    if (second_colon != std::string_view::npos &&
        second_colon + 1 < tag.size()) {
      tags.component = "nng.";
      tags.component.append(
          tag.substr(first_colon + 1, second_colon - first_colon - 1));
      tags.action = std::string(tag.substr(second_colon + 1));
      return tags;
    }
  }

  tags.component = std::string(tag.substr(0, first_colon));
  tags.action = std::string(tag.substr(first_colon + 1));
  return tags;
}

void append_padded(spdlog::string_view_t text,
                   const spdlog::details::padding_info &padding,
                   spdlog::memory_buf_t &dest) {
  if (!padding.enabled()) {
    spdlog::details::fmt_helper::append_string_view(text, dest);
    return;
  }

  const std::size_t content_size = text.size();
  if (content_size >= padding.width_) {
    const auto size = padding.truncate_ ? padding.width_ : content_size;
    spdlog::details::fmt_helper::append_string_view(
        spdlog::string_view_t{text.data(), size}, dest);
    return;
  }

  const std::size_t spaces = padding.width_ - content_size;
  if (padding.side_ == spdlog::details::padding_info::pad_side::left) {
    append_spaces(spaces, dest);
    spdlog::details::fmt_helper::append_string_view(text, dest);
    return;
  }

  if (padding.side_ == spdlog::details::padding_info::pad_side::right) {
    spdlog::details::fmt_helper::append_string_view(text, dest);
    append_spaces(spaces, dest);
    return;
  }

  const std::size_t left_spaces = spaces / 2;
  append_spaces(left_spaces, dest);
  spdlog::details::fmt_helper::append_string_view(text, dest);
  append_spaces(spaces - left_spaces, dest);
}

class zpp_thread_id_formatter final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg &msg, const std::tm &,
              spdlog::memory_buf_t &dest) override {
    std::size_t thread_id = 0;
    spdlog::string_view_t message;
    if (!read_zpp_tid_payload(msg.payload, thread_id, message)) {
      thread_id = msg.thread_id;
    }
    spdlog::memory_buf_t formatted;
    append_short_thread_id(thread_id, formatted);
    append_padded(spdlog::string_view_t{formatted.data(), formatted.size()},
                  padinfo_, dest);
  }

  std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<zpp_thread_id_formatter>();
  }
};

class zpp_payload_formatter final : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg &msg, const std::tm &,
              spdlog::memory_buf_t &dest) override {
    std::size_t thread_id = 0;
    spdlog::string_view_t message;
    if (read_zpp_tid_payload(msg.payload, thread_id, message)) {
      append_padded(message, padinfo_, dest);
      return;
    }
    append_padded(msg.payload, padinfo_, dest);
  }

  std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<zpp_payload_formatter>();
  }
};

class jsonl_formatter final : public spdlog::formatter {
public:
  explicit jsonl_formatter(const z::log::config &cfg)
      : service_(cfg.service), node_(cfg.node), run_id_(cfg.run_id) {}

  void format(const spdlog::details::log_msg &msg,
              spdlog::memory_buf_t &dest) override {
    std::size_t thread_id = 0;
    spdlog::string_view_t message;
    if (!read_zpp_tid_payload(msg.payload, thread_id, message)) {
      thread_id = msg.thread_id;
      message = msg.payload;
    }
    const auto tags = parse_log_tags(message);
    bool first = true;
    dest.push_back('{');
    append_json_field("ts", make_iso_timestamp(msg.time), dest, first);
    append_json_field("level", level_name(msg.level), dest, first);
    append_json_field("logger", msg.logger_name, dest, first);
    append_json_field("service", service_, dest, first);
    append_json_field("node", node_, dest, first);
    append_json_field("run_id", run_id_, dest, first);
    append_json_number_field(
        "pid", static_cast<std::size_t>(spdlog::details::os::pid()), dest,
        first);
    append_json_number_field("tid", thread_id, dest, first);
    append_json_field("file", msg.source.filename, dest, first);
    append_json_number_field("line", static_cast<std::size_t>(msg.source.line),
                             dest, first);
    append_json_field("function", msg.source.funcname, dest, first);
    append_json_field("component", tags.component, dest, first);
    append_json_field("action", tags.action, dest, first);
    append_json_field("message", message, dest, first);
    dest.push_back('}');
    dest.push_back('\n');
  }

  std::unique_ptr<spdlog::formatter> clone() const override {
    return std::make_unique<jsonl_formatter>(*this);
  }

private:
  std::string service_;
  std::string node_;
  std::string run_id_;
};

std::unique_ptr<spdlog::formatter>
make_pattern_formatter(const std::string &pattern) {
  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<zpp_thread_id_formatter>('t')
      .add_flag<zpp_payload_formatter>('v')
      .set_pattern(pattern);
  return formatter;
}

std::unique_ptr<spdlog::formatter>
make_sink_formatter(z::log::sink_format format, const std::string &pattern,
                    const z::log::config &cfg) {
  if (format == z::log::sink_format::jsonl) {
    return std::make_unique<jsonl_formatter>(cfg);
  }
  return make_pattern_formatter(pattern);
}

std::shared_ptr<spdlog::logger> make_fallback_logger() {
  auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  auto logger = std::make_shared<spdlog::logger>("zpp", sink);
  logger->set_level(spdlog::level::off);
  return logger;
}

spdlog::level::level_enum normalize_level(int level) {
  switch (level) {
  case 0:
    return spdlog::level::trace;
  case 1:
    return spdlog::level::debug;
  case 2:
    return spdlog::level::info;
  case 3:
    return spdlog::level::warn;
  case 4:
    return spdlog::level::err;
  case 5:
    return spdlog::level::critical;
  case 6:
    return spdlog::level::off;
  default:
    return spdlog::level::info;
  }
}

bool should_log_level(spdlog::level::level_enum level) noexcept {
  auto *logger = g_spd_log.get();
  return logger && level != spdlog::level::off &&
         (logger->should_log(level) || logger->should_backtrace());
}

void create_parent_directory(const std::string &file_name) {
  const std::filesystem::path path(file_name);
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
}

class important_direct_sink final : public spdlog::sinks::sink {
public:
  explicit important_direct_sink(spdlog::sink_ptr inner)
      : inner_(std::move(inner)) {}

  void log(const spdlog::details::log_msg &msg) override { inner_->log(msg); }

  void flush() override { inner_->flush(); }

  void set_pattern(const std::string &pattern) override {
    inner_->set_pattern(pattern);
  }

  void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override {
    inner_->set_formatter(std::move(formatter));
  }

private:
  spdlog::sink_ptr inner_;
};

struct sink_bundle {
  std::vector<spdlog::sink_ptr> runtime_sinks;
  spdlog::sink_ptr important_sink;
};

void append_zpp_log_payload(spdlog::string_view_t msg,
                            spdlog::memory_buf_t &payload) {
  spdlog::details::fmt_helper::append_string_view(zpp_tid_prefix, payload);
  spdlog::details::fmt_helper::append_int(
      static_cast<std::size_t>(z::tid::id()), payload);
  payload.push_back(zpp_tid_suffix);
  spdlog::details::fmt_helper::append_string_view(msg, payload);
}

sink_bundle make_sinks(const z::log::config &cfg) {
  sink_bundle bundle;
  const std::string pattern =
      cfg.pattern.empty() ? "%d %H:%M:%S.%f %t %^%L%$: %v\t%g:%#" : cfg.pattern;
  if (cfg.console) {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink->set_formatter(
        make_sink_formatter(z::log::sink_format::pattern, pattern, cfg));
    bundle.runtime_sinks.push_back(std::move(sink));
  }
  if (cfg.rotating_file) {
    const auto max_size = cfg.max_file_size_mb * 1024 * 1024;
    create_parent_directory(cfg.file_name);
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        cfg.file_name, max_size, cfg.max_files, cfg.rotate_on_open);
    sink->set_formatter(
        make_sink_formatter(z::log::sink_format::pattern, pattern, cfg));
    bundle.runtime_sinks.push_back(std::move(sink));
  }
  if (cfg.important_file.enabled) {
    const std::string important_file_name =
        cfg.important_file.file_name.empty()
            ? cfg.logger_name + ".important.jsonl"
            : cfg.important_file.file_name;
    const auto max_size = cfg.important_file.max_file_size_mb * 1024 * 1024;
    create_parent_directory(important_file_name);
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        important_file_name, max_size, cfg.important_file.max_files,
        cfg.important_file.rotate_on_open);
    const std::string important_pattern = cfg.important_file.pattern.empty()
                                              ? pattern
                                              : cfg.important_file.pattern;
    sink->set_level(cfg.important_file.level);
    sink->set_formatter(
        make_sink_formatter(cfg.important_file.format, important_pattern, cfg));
    bundle.important_sink = sink;
    bundle.runtime_sinks.push_back(std::move(sink));
  }

  if (bundle.runtime_sinks.empty()) {
    bundle.runtime_sinks.push_back(
        std::make_shared<spdlog::sinks::null_sink_mt>());
  }
  return bundle;
}

void trace_vprintf(int level, const char *file, int line, const char *fn,
                   const char *msg, va_list args) {
  const auto spd_level = normalize_level(level);
  if (!should_log_level(spd_level)) {
    return;
  }

  if (!msg) {
    g_spd_log->log(spdlog::source_loc{file, line, fn}, spd_level, "");
    return;
  }

  thread_local std::array<char, 4096> buffer;
  va_list args_copy;
  va_copy(args_copy, args);
  const int result =
      std::vsnprintf(buffer.data(), buffer.size(), msg, args_copy);
  va_end(args_copy);

  if (result < 0) {
    g_spd_log->log(spdlog::source_loc{file, line, fn}, spd_level,
                   "[format error]");
    return;
  }

  const std::size_t used = result >= static_cast<int>(buffer.size())
                               ? buffer.size() - 1
                               : static_cast<std::size_t>(result);
  buffer[used] = '\0';
  z::log::log_preformatted(spdlog::source_loc{file, line, fn}, spd_level,
                           spdlog::string_view_t{buffer.data(), used});
}

} // namespace

std::shared_ptr<spdlog::logger> g_spd_log{make_fallback_logger()};
std::shared_ptr<spdlog::logger> g_spd_important_log{make_fallback_logger()};

NSB_ZPP
namespace log {

void init(const config &cfg) {
  auto sinks = make_sinks(cfg);
  if (cfg.async) {
    g_spd_pool =
        std::make_shared<spdlog::details::thread_pool>(cfg.queue_size, 1);
    g_spd_log = std::make_shared<spdlog::async_logger>(
        cfg.logger_name, sinks.runtime_sinks.begin(), sinks.runtime_sinks.end(),
        g_spd_pool, spdlog::async_overflow_policy::block);
  } else {
    g_spd_pool.reset();
    g_spd_log = std::make_shared<spdlog::logger>(cfg.logger_name,
                                                 sinks.runtime_sinks.begin(),
                                                 sinks.runtime_sinks.end());
  }

  if (sinks.important_sink) {
    auto direct_sink =
        std::make_shared<important_direct_sink>(sinks.important_sink);
    if (cfg.async) {
      g_spd_important_log = std::make_shared<spdlog::async_logger>(
          cfg.logger_name + ".important", std::move(direct_sink), g_spd_pool,
          spdlog::async_overflow_policy::block);
    } else {
      g_spd_important_log = std::make_shared<spdlog::logger>(
          cfg.logger_name + ".important", std::move(direct_sink));
    }
    g_spd_important_log->set_level(spdlog::level::trace);
  } else {
    g_spd_important_log = make_fallback_logger();
  }

  g_spd_log->set_level(cfg.level);
  g_spd_log->flush_on(cfg.flush_level);
  spdlog::set_default_logger(g_spd_log);
  if (cfg.flush_interval.count() > 0) {
    spdlog::flush_every(cfg.flush_interval);
  }
  spdlog::cfg::load_env_levels();
}

void shutdown() noexcept {
  try {
    auto logger = std::move(g_spd_log);
    auto important_logger = std::move(g_spd_important_log);
    auto pool = std::move(g_spd_pool);
    if (logger) {
      logger->flush();
    }
    if (important_logger) {
      important_logger->flush();
    }
    spdlog::shutdown();
    pool.reset();
    g_spd_log = make_fallback_logger();
    g_spd_important_log = make_fallback_logger();
  } catch (...) {
    g_spd_log = make_fallback_logger();
    g_spd_important_log = make_fallback_logger();
    g_spd_pool.reset();
  }
}

void set_level(spdlog::level::level_enum level) { g_spd_log->set_level(level); }

bool should_log_important() noexcept {
  auto *important_logger = g_spd_important_log.get();
  return should_log_level(spdlog::level::info) ||
         (important_logger &&
          important_logger->should_log(spdlog::level::info));
}

void log_preformatted(spdlog::source_loc source, spdlog::level::level_enum lvl,
                      spdlog::string_view_t msg) {
  spdlog::memory_buf_t payload;
  append_zpp_log_payload(msg, payload);

  g_spd_log->log(source, lvl,
                 spdlog::string_view_t{payload.data(), payload.size()});
}

void log_important_preformatted(spdlog::source_loc source,
                                spdlog::string_view_t msg) {
  spdlog::memory_buf_t payload;
  append_zpp_log_payload(msg, payload);
  const spdlog::string_view_t payload_view{payload.data(), payload.size()};
  g_spd_log->log(source, spdlog::level::info, payload_view);
  g_spd_important_log->log(source, spdlog::level::info, payload_view);
}

bool should_trace_printf(int level) noexcept {
  return should_log_level(normalize_level(level));
}

void trace_printf(int level, const char *file, int line, const char *fn,
                  const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  trace_vprintf(level, file, line, fn, msg, args);
  va_end(args);
}

} // namespace log

NSE_ZPP
