/**
 * @file spdlog.h
 * @brief Thin zpp wrapper around the native spdlog hot path.
 */
#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#ifdef FMT_USE_INT128
#undef FMT_USE_INT128
#define FMT_USE_INT128 0
#endif

#include <chrono>
#include <cstddef>
#include <locale>
#include <memory>
#include <string>
#include <utility>

#include <spdlog/common.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/spdlog.h>

#include "namespace.h"

extern std::shared_ptr<spdlog::logger> g_spd_log;

NSB_ZPP
namespace log {

/// Log sink output format.
enum class sink_format {
  /// Format records through the configured spdlog pattern.
  pattern,
  /// Write one structured JSON object per log record.
  jsonl,
};

/// Additional rotating file sink configuration.
struct file_sink_config {
  /// Enables this file sink when true.
  bool enabled{false};
  /// Output file name.
  std::string file_name;
  /// Maximum size of each rotated file in MiB.
  std::size_t max_file_size_mb{32};
  /// Number of rotated files to keep.
  std::size_t max_files{3};
  /// Truncate/rotate the active file when opening the sink.
  bool rotate_on_open{true};
  /// Minimum level accepted by this sink.
  spdlog::level::level_enum level{spdlog::level::warn};
  /// Sink output format.
  sink_format format{sink_format::jsonl};
  /// Pattern used when @ref format is @ref sink_format::pattern.
  std::string pattern;
};

/// Native spdlog initialization configuration.
struct config {
  /// Use spdlog async logger when true.
  bool async{false};
  /// Enable colored stdout logging.
  bool console{true};
  /// Enable the legacy rotating runtime file sink.
  bool rotating_file{false};
  /// Rotate/truncate the legacy runtime file sink on open.
  bool rotate_on_open{true};
  /// Legacy runtime file name.
  std::string file_name{"server.log"};
  /// Legacy runtime file maximum size in MiB.
  std::size_t max_file_size_mb{32};
  /// Legacy runtime rotated file count.
  std::size_t max_files{3};
  /// Async logger queue size.
  std::size_t queue_size{8192};
  /// Global periodic flush interval.
  std::chrono::seconds flush_interval{2};
  /// Logger minimum level.
  spdlog::level::level_enum level{spdlog::level::trace};
  /// Flush records at this level or higher.
  spdlog::level::level_enum flush_level{spdlog::level::warn};
  /// Logger name.
  std::string logger_name{"zpp"};
  /// Legacy text pattern.
  std::string pattern{"%d %H:%M:%S.%f %t %^%L%$: %v\t%g:%#"};
  /// Logical service name written to JSONL sinks.
  std::string service;
  /// Logical node name written to JSONL sinks.
  std::string node;
  /// Per-run identifier written to JSONL sinks.
  std::string run_id;
  /// AI-agent focused important log sink.
  file_sink_config important_file;
};

void init(const config &cfg = config{});
void shutdown() noexcept;
void set_level(spdlog::level::level_enum level);
void log_preformatted(spdlog::source_loc source, spdlog::level::level_enum lvl,
                      spdlog::string_view_t msg);
void log_important_preformatted(spdlog::source_loc source,
                                spdlog::string_view_t msg);
bool should_log_important() noexcept;

} // namespace log
NSE_ZPP

#define spd_begin(x) g_spd_log->enable_backtrace(x)
#define spd_end g_spd_log->dump_backtrace

template <typename... Args>
inline void spd_logx(spdlog::source_loc source, spdlog::level::level_enum lvl,
                     spdlog::format_string_t<Args...> fmt_str, Args &&...args) {
  spdlog::memory_buf_t buffer;
#ifdef SPDLOG_USE_STD_FORMAT
  fmt_lib::vformat_to(std::back_inserter(buffer),
                      spdlog::details::to_string_view(fmt_str),
                      fmt_lib::make_format_args(args...));
#else
  fmt::vformat_to(fmt::appender(buffer),
                  spdlog::details::to_string_view(fmt_str),
                  fmt::make_format_args(args...));
#endif
  z::log::log_preformatted(source, lvl,
                           spdlog::string_view_t{buffer.data(), buffer.size()});
}

template <typename... Args>
inline void spd_logx_important(spdlog::source_loc source,
                               spdlog::format_string_t<Args...> fmt_str,
                               Args &&...args) {
  spdlog::memory_buf_t buffer;
#ifdef SPDLOG_USE_STD_FORMAT
  fmt_lib::vformat_to(std::back_inserter(buffer),
                      spdlog::details::to_string_view(fmt_str),
                      fmt_lib::make_format_args(args...));
#else
  fmt::vformat_to(fmt::appender(buffer),
                  spdlog::details::to_string_view(fmt_str),
                  fmt::make_format_args(args...));
#endif
  z::log::log_important_preformatted(
      source, spdlog::string_view_t{buffer.data(), buffer.size()});
}

inline bool spd_should_log(spdlog::level::level_enum lvl) noexcept {
  auto *logger = g_spd_log.get();
  return logger && lvl != spdlog::level::off &&
         (logger->should_log(lvl) || logger->should_backtrace());
}

#define spd_logx_if(lvl, ...)                                                  \
  (spd_should_log(lvl)                                                         \
       ? spd_logx(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, lvl,   \
                  __VA_ARGS__)                                                 \
       : (void)0)

#define spd_logx_important_if(...)                                             \
  (z::log::should_log_important()                                              \
       ? spd_logx_important(                                                   \
             spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__},             \
             __VA_ARGS__)                                                      \
       : (void)0)

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
#define spd_inf(...) spd_logx_if(spdlog::level::info, __VA_ARGS__)
#define spd_imp(...) spd_logx_important_if(__VA_ARGS__)
#else
#define spd_inf(...) (void)0
#define spd_imp(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
#define spd_err(...) spd_logx_if(spdlog::level::err, __VA_ARGS__)
#else
#define spd_err(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
#define spd_war(...) spd_logx_if(spdlog::level::warn, __VA_ARGS__)
#else
#define spd_war(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define spd_dbg(...) spd_logx_if(spdlog::level::debug, __VA_ARGS__)
#else
#define spd_dbg(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define spd_trc(...) spd_logx_if(spdlog::level::trace, __VA_ARGS__)
#else
#define spd_trc(...) (void)0
#endif

#ifdef _DEBUG
#define spd_mark() g_spd_log->info("{}:{}", __PRETTY_FUNCTION__, __LINE__)
#else
#define spd_mark() (void)0
#endif

#define CTS(x) fmt::format(std::locale("en_US.UTF-8"), "{:L}", (x))
