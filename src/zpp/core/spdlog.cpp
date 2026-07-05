/**
 * @file spdlog.cpp
 * @brief Native-spdlog implementation for zpp logging initialization.
 */
#include <array>
#include <cstdarg>
#include <cstdio>
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
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
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

std::unique_ptr<spdlog::formatter>
make_pattern_formatter(const std::string &pattern) {
  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<zpp_thread_id_formatter>('t')
      .add_flag<zpp_payload_formatter>('v')
      .set_pattern(pattern);
  return formatter;
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

std::vector<spdlog::sink_ptr> make_sinks(const z::log::config &cfg) {
  std::vector<spdlog::sink_ptr> sinks;
  if (cfg.console) {
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  }
  if (cfg.rotating_file) {
    const auto max_size = cfg.max_file_size_mb * 1024 * 1024;
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        cfg.file_name, max_size, cfg.max_files, cfg.rotate_on_open));
  }
  if (sinks.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
  }

  const std::string pattern =
      cfg.pattern.empty() ? "%d %H:%M:%S.%f %t %^%L%$: %v\t%g:%#" : cfg.pattern;
  for (auto &sink : sinks) {
    sink->set_formatter(make_pattern_formatter(pattern));
  }
  return sinks;
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

NSB_ZPP
namespace log {

void init(const config &cfg) {
  auto sinks = make_sinks(cfg);
  if (cfg.async) {
    g_spd_pool =
        std::make_shared<spdlog::details::thread_pool>(cfg.queue_size, 1);
    g_spd_log = std::make_shared<spdlog::async_logger>(
        cfg.logger_name, sinks.begin(), sinks.end(), g_spd_pool,
        spdlog::async_overflow_policy::block);
  } else {
    g_spd_pool.reset();
    g_spd_log = std::make_shared<spdlog::logger>(cfg.logger_name, sinks.begin(),
                                                 sinks.end());
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
    auto pool = std::move(g_spd_pool);
    if (logger) {
      logger->flush();
    }
    spdlog::shutdown();
    pool.reset();
    g_spd_log = make_fallback_logger();
  } catch (...) {
    g_spd_log = make_fallback_logger();
    g_spd_pool.reset();
  }
}

void set_level(spdlog::level::level_enum level) { g_spd_log->set_level(level); }

void log_preformatted(spdlog::source_loc source, spdlog::level::level_enum lvl,
                      spdlog::string_view_t msg) {
  spdlog::memory_buf_t payload;
  spdlog::details::fmt_helper::append_string_view(zpp_tid_prefix, payload);
  spdlog::details::fmt_helper::append_int(
      static_cast<std::size_t>(z::tid::id()), payload);
  payload.push_back(zpp_tid_suffix);
  spdlog::details::fmt_helper::append_string_view(msg, payload);

  g_spd_log->log(source, lvl,
                 spdlog::string_view_t{payload.data(), payload.size()});
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
