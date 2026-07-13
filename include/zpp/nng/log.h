#pragma once

#include "defs.h"

#include <string_view>
#include <utility>

#include <zpp/spdlog.h>

/// Captures the current source location for zpp NNG logging helpers.
#define ZPP_NNG_SOURCE_LOC                                                     \
  spdlog::source_loc { __FILE__, __LINE__, __FUNCTION__ }

NSB_NNG

/// NNG log component tag for socket-level operations.
inline constexpr char cmp_socket[] = "socket";
/// NNG log component tag for listener wrapper operations.
inline constexpr char cmp_listener[] = "listener";
/// NNG log component tag for dialer wrapper operations.
inline constexpr char cmp_dialer[] = "dialer";
/// NNG log component tag for pipe wrapper operations.
inline constexpr char cmp_pipe[] = "pipe";
/// NNG log component tag for pipe notification operations.
inline constexpr char cmp_pipe_notify[] = "pipe-notify";
/// NNG log component tag for PUB/SUB device operations.
inline constexpr char cmp_pub_sub_device[] = "pub-sub-device";
/// NNG log component tag for endpoint validation and orchestration.
inline constexpr char cmp_endpoint[] = "endpoint";
/// NNG log component tag for publisher operations.
inline constexpr char cmp_publisher[] = "publisher";
/// NNG log component tag for subscriber operations.
inline constexpr char cmp_subscriber[] = "subscriber";
/// NNG log component tag for push protocol operations.
inline constexpr char cmp_pusher[] = "pusher";
/// NNG log component tag for pull protocol operations.
inline constexpr char cmp_puller[] = "puller";
/// NNG log component tag for request protocol operations.
inline constexpr char cmp_requester[] = "requester";
/// NNG log component tag for reply protocol operations.
inline constexpr char cmp_replier[] = "replier";
/// NNG log component tag for survey protocol operations.
inline constexpr char cmp_surveyor[] = "surveyor";
/// NNG log component tag for respondent protocol operations.
inline constexpr char cmp_respondent[] = "respondent";
/// NNG log component tag for bus protocol operations.
inline constexpr char cmp_bus[] = "bus";
/// NNG log component tag for pair protocol operations.
inline constexpr char cmp_pair[] = "pair";
/// NNG log component tag for runtime operations.
inline constexpr char cmp_runtime[] = "runtime";
/// NNG log component tag for AIO operations.
inline constexpr char cmp_aio[] = "aio";
/// NNG log component tag for context operations.
inline constexpr char cmp_context[] = "context";
/// NNG log component tag for message operations.
inline constexpr char cmp_message[] = "message";
/// NNG log component tag used by logging tests.
inline constexpr char cmp_test[] = "test";

/// NNG log action tag for create operations.
inline constexpr char act_create[] = "create";
/// NNG log action tag for start operations.
inline constexpr char act_start[] = "start";
/// NNG log action tag for asynchronous start operations.
inline constexpr char act_start_aio[] = "start-aio";
/// NNG log action tag for close operations.
inline constexpr char act_close[] = "close";
/// NNG log action tag for listen operations.
inline constexpr char act_listen[] = "listen";
/// NNG log action tag for dial operations.
inline constexpr char act_dial[] = "dial";
/// NNG log action tag for validation failures.
inline constexpr char act_validate[] = "validate";
/// NNG log action tag for device operations.
inline constexpr char act_device[] = "device";
/// NNG log action tag for AIO allocation.
inline constexpr char act_aio_alloc[] = "aio-alloc";
/// NNG log action tag for initialization.
inline constexpr char act_init[] = "init";
/// NNG log action tag for finalization.
inline constexpr char act_fini[] = "fini";
/// NNG log action tag used by source-location tests.
inline constexpr char act_source_location[] = "source-location";

/// NNG log action tag for set_bool option operations.
inline constexpr char act_set_bool[] = "set_bool";
/// NNG log action tag for get_bool option operations.
inline constexpr char act_get_bool[] = "get_bool";
/// NNG log action tag for set_int option operations.
inline constexpr char act_set_int[] = "set_int";
/// NNG log action tag for get_int option operations.
inline constexpr char act_get_int[] = "get_int";
/// NNG log action tag for set_size option operations.
inline constexpr char act_set_size[] = "set_size";
/// NNG log action tag for get_size option operations.
inline constexpr char act_get_size[] = "get_size";
/// NNG log action tag for set_uint64 option operations.
inline constexpr char act_set_uint64[] = "set_uint64";
/// NNG log action tag for get_uint64 option operations.
inline constexpr char act_get_uint64[] = "get_uint64";
/// NNG log action tag for set_string option operations.
inline constexpr char act_set_string[] = "set_string";
/// NNG log action tag for get_string option operations.
inline constexpr char act_get_string[] = "get_string";
/// NNG log action tag for set_ms option operations.
inline constexpr char act_set_ms[] = "set_ms";
/// NNG log action tag for get_ms option operations.
inline constexpr char act_get_ms[] = "get_ms";
/// NNG log action tag for set_addr option operations.
inline constexpr char act_set_addr[] = "set_addr";
/// NNG log action tag for get_addr option operations.
inline constexpr char act_get_addr[] = "get_addr";
/// NNG log action tag for set_tls option operations.
inline constexpr char act_set_tls[] = "set_tls";
/// NNG log action tag for get_tls option operations.
inline constexpr char act_get_tls[] = "get_tls";
/// NNG log action tag for set_security_descriptor operations.
inline constexpr char act_set_security_descriptor[] = "set_security_descriptor";
/// NNG log action tag for get_url operations.
inline constexpr char act_get_url[] = "get_url";
/// NNG log action tag for send operations.
inline constexpr char act_send[] = "send";
/// NNG log action tag for receive operations.
inline constexpr char act_recv[] = "recv";
/// NNG log action tag for message operations.
inline constexpr char act_message[] = "message";
/// NNG log action tag for configuration operations.
inline constexpr char act_configure[] = "configure";
/// NNG log action tag for managed context-pool operations.
inline constexpr char act_context_pool[] = "context-pool";
/// NNG log action tag for asynchronous callback dispatch.
inline constexpr char act_callback[] = "callback";
/// NNG log action tag for NNG internal logger bridge messages.
inline constexpr char act_logger[] = "logger";

/// Formats an NNG-scoped message with component and action tags.
template <typename... Args>
inline void nng2format(spdlog::memory_buf_t &buffer, std::string_view component,
                       std::string_view action,
                       spdlog::format_string_t<Args...> fmt_str,
                       Args &&...args) {
#ifdef SPDLOG_USE_STD_FORMAT
  fmt_lib::format_to(std::back_inserter(buffer), "[nng:{}:{}] ", component,
                     action);
  fmt_lib::vformat_to(std::back_inserter(buffer),
                      spdlog::details::to_string_view(fmt_str),
                      fmt_lib::make_format_args(args...));
#else
  fmt::format_to(fmt::appender(buffer), "[nng:{}:{}] ", component, action);
  fmt::vformat_to(fmt::appender(buffer),
                  spdlog::details::to_string_view(fmt_str),
                  fmt::make_format_args(args...));
#endif
}

/// Logs an NNG-scoped message with component and action tags.
template <typename... Args>
inline void nng2log(spdlog::source_loc source, spdlog::level::level_enum lvl,
                    std::string_view component, std::string_view action,
                    spdlog::format_string_t<Args...> fmt_str, Args &&...args) {
  spdlog::memory_buf_t buffer;
  nng2format(buffer, component, action, fmt_str, std::forward<Args>(args)...);
  z::log::log_preformatted(source, lvl,
                           spdlog::string_view_t{buffer.data(), buffer.size()});
}

/// Logs an NNG-scoped important message with component and action tags.
template <typename... Args>
inline void
nng2log_important(spdlog::source_loc source, std::string_view component,
                  std::string_view action,
                  spdlog::format_string_t<Args...> fmt_str, Args &&...args) {
  spdlog::memory_buf_t buffer;
  nng2format(buffer, component, action, fmt_str, std::forward<Args>(args)...);
  z::log::log_important_preformatted(
      source, spdlog::string_view_t{buffer.data(), buffer.size()});
}

NSE_NNG

#define nng2log_if(lvl, component, action, ...)                                \
  (spd_should_log(lvl)                                                         \
       ? z::nng::nng2log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, \
                         lvl, component, action, __VA_ARGS__)                  \
       : (void)0)

#define nng2imp_if(component, action, ...)                                     \
  (z::log::should_log_important()                                              \
       ? z::nng::nng2log_important(                                            \
             spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, component,  \
             action, __VA_ARGS__)                                              \
       : (void)0)

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
#define nng2inf(component, action, ...)                                        \
  nng2log_if(spdlog::level::info, component, action, __VA_ARGS__)
#define nng2imp(component, action, ...)                                        \
  nng2imp_if(component, action, __VA_ARGS__)
#else
#define nng2inf(component, action, ...) (void)0
#define nng2imp(component, action, ...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
#define nng2errf(component, action, ...)                                       \
  nng2log_if(spdlog::level::err, component, action, __VA_ARGS__)
#else
#define nng2errf(component, action, ...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
#define nng2war(component, action, ...)                                        \
  nng2log_if(spdlog::level::warn, component, action, __VA_ARGS__)
#else
#define nng2war(component, action, ...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define nng2dbg(component, action, ...)                                        \
  nng2log_if(spdlog::level::debug, component, action, __VA_ARGS__)
#else
#define nng2dbg(component, action, ...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define nng2trc(component, action, ...)                                        \
  nng2log_if(spdlog::level::trace, component, action, __VA_ARGS__)
#else
#define nng2trc(component, action, ...) (void)0
#endif

#define nng2err(component, action, rv)                                         \
  do {                                                                         \
    const int nng2_rv = static_cast<int>(rv);                                  \
    if (nng2_rv != 0) {                                                        \
      nng2errf(component, action, "error={} message={}", nng2_rv,              \
               nng_strerror(static_cast<::nng_err>(nng2_rv)));                 \
    }                                                                          \
  } while (false)

#define nng2err_url(component, action, rv, url)                                \
  do {                                                                         \
    const int nng2_rv = static_cast<int>(rv);                                  \
    const auto *nng2_url = (url);                                              \
    if (nng2_rv != 0) {                                                        \
      nng2errf(component, action, "error={} message={} url={}", nng2_rv,       \
               nng_strerror(static_cast<::nng_err>(nng2_rv)),                  \
               nng2_url == nullptr ? "<null>" : nng2_url);                     \
    }                                                                          \
  } while (false)

#define nng2opt_err(component, action, rv, option)                             \
  do {                                                                         \
    const int nng2_rv = static_cast<int>(rv);                                  \
    const auto *nng2_option = (option);                                        \
    if (nng2_rv != 0) {                                                        \
      nng2errf(component, action, "option={} error={} message={}",             \
               nng2_option == nullptr ? "<null>" : nng2_option, nng2_rv,       \
               nng_strerror(static_cast<::nng_err>(nng2_rv)));                 \
    }                                                                          \
  } while (false)
