#include <zpp/nng/nng.h>

#include <zpp/nng/log.h>
#include <zpp/spd_log.h>

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace z::nng {
namespace {

spdlog::level::level_enum
map_nng_level_to_spd_level(nng_log_level level) noexcept {
  switch (level) {
  case NNG_LOG_ERR:
    return spdlog::level::err;
  case NNG_LOG_WARN:
    return spdlog::level::warn;
  case NNG_LOG_NOTICE:
  case NNG_LOG_INFO:
    return spdlog::level::info;
  case NNG_LOG_DEBUG:
    return spdlog::level::debug;
  case NNG_LOG_NONE:
  default:
    return spdlog::level::off;
  }
}

nng_log_level map_zpp_level_to_nng_level() noexcept {
  if (z::log::should_trace_printf(1)) {
    return NNG_LOG_DEBUG;
  }
  if (z::log::should_trace_printf(2)) {
    return NNG_LOG_INFO;
  }
  if (z::log::should_trace_printf(3)) {
    return NNG_LOG_WARN;
  }
  if (z::log::should_trace_printf(4) || z::log::should_trace_printf(5)) {
    return NNG_LOG_ERR;
  }
  return NNG_LOG_NONE;
}

struct suppressed_bridge_log {
  std::chrono::steady_clock::time_point last_emit;
  std::size_t suppressed_count{0};
};

struct bridge_log_decision {
  bool should_emit{true};
  std::size_t suppressed_count{0};
};

std::mutex &bridge_log_suppression_mutex() noexcept {
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::string, suppressed_bridge_log> &
bridge_log_suppression_state() {
  static std::unordered_map<std::string, suppressed_bridge_log> state;
  return state;
}

std::string make_bridge_log_key(nng_log_level level, nng_log_facility facility,
                                std::string_view msgid,
                                std::string_view message) {
  std::string key;
  key.reserve(msgid.size() + message.size() + 32);
  key.append(std::to_string(static_cast<int>(level)));
  key.push_back('\n');
  key.append(std::to_string(static_cast<int>(facility)));
  key.push_back('\n');
  key.append(msgid);
  key.push_back('\n');
  key.append(message);
  return key;
}

bridge_log_decision decide_bridge_log(nng_log_level level,
                                      nng_log_facility facility,
                                      std::string_view msgid,
                                      std::string_view message) {
  static constexpr auto min_emit_interval = std::chrono::seconds{600};
  if (level == NNG_LOG_NONE) {
    return {};
  }

  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock{bridge_log_suppression_mutex()};
  auto &state = bridge_log_suppression_state();
  auto &entry = state[make_bridge_log_key(level, facility, msgid, message)];
  if (entry.last_emit == std::chrono::steady_clock::time_point{}) {
    entry.last_emit = now;
    return {};
  }
  if (now - entry.last_emit < min_emit_interval) {
    ++entry.suppressed_count;
    return {.should_emit = false};
  }

  const auto suppressed_count = entry.suppressed_count;
  entry.last_emit = now;
  entry.suppressed_count = 0;
  return {.should_emit = true, .suppressed_count = suppressed_count};
}

void zpp_nng_logger(nng_log_level level, nng_log_facility facility,
                    const char *msgid, const char *msg) {
  const std::string_view msgid_view{msgid == nullptr ? "-" : msgid};
  const std::string_view msg_view{msg == nullptr ? "" : msg};
  const auto decision = decide_bridge_log(level, facility, msgid_view, msg_view);
  if (!decision.should_emit) {
    return;
  }

  // spdlog treats line 0 as an empty source location, so use a synthetic
  // line 1.
  if (decision.suppressed_count > 0) {
    nng2log(spdlog::source_loc{"nng-runtime", 1, "nng_logger"},
            map_nng_level_to_spd_level(level), cmp_runtime, act_logger,
            "facility={} msgid={} suppressed_repeats={} {}",
            static_cast<int>(facility), msgid_view, decision.suppressed_count,
            msg_view);
  } else {
    nng2log(spdlog::source_loc{"nng-runtime", 1, "nng_logger"},
            map_nng_level_to_spd_level(level), cmp_runtime, act_logger,
            "facility={} msgid={} {}", static_cast<int>(facility), msgid_view,
            msg_view);
  }
}

std::mutex &logger_mutex() noexcept {
  static std::mutex mutex;
  return mutex;
}

int &logger_ref_count() noexcept {
  static int count = 0;
  return count;
}

void install_logger() {
  std::lock_guard<std::mutex> lock(logger_mutex());
  auto &count = logger_ref_count();
  if (count == 0) {
    nng_log_set_level(map_zpp_level_to_nng_level());
    nng_log_set_logger(&zpp_nng_logger);
  }
  ++count;
}

void uninstall_logger() noexcept {
  std::lock_guard<std::mutex> lock(logger_mutex());
  auto &count = logger_ref_count();
  if (count <= 0) {
    return;
  }
  --count;
  if (count == 0) {
    nng_log_set_logger(nng_null_logger);
  }
}

} // namespace

nng::nng() {
  nng_init_params params{0};
  init(params);
}

nng::nng(const nng_init_params &params) { init(params); }

nng::nng(std::int16_t num_task_thrs, std::int16_t max_task_thrs,
         std::int16_t num_expire_thrs, std::int16_t max_expire_thrs,
         std::int16_t num_poller_thrs, std::int16_t max_poller_thrs,
         std::int16_t num_resolver_thrs) {
  nng_init_params params{0};
  params.num_task_threads = num_task_thrs;
  params.max_task_threads = max_task_thrs;
  params.num_expire_threads = num_expire_thrs;
  params.max_expire_threads = max_expire_thrs;
  params.num_poller_threads = num_poller_thrs;
  params.max_poller_threads = max_poller_thrs;
  params.num_resolver_threads = num_resolver_thrs;
  init(params);
}

nng::~nng() {
  uninstall_logger();
  nng_fini();
  nng2inf(cmp_runtime, act_fini, "done");
}

void nng::init(const nng_init_params &params) {
  const auto err = nng_init(&params);
  if (err != NNG_OK) {
    const auto result = static_cast<int>(err);
    nng2errf(cmp_runtime, act_init, "failed[{}]: {}", result,
             nng_strerror(err));
    std::exit(EXIT_FAILURE);
  }
  install_logger();
  nng2inf(cmp_runtime, act_init,
          "num_task_threads[{}] max_task_threads[{}] num_expire_threads[{}] "
          "max_expire_threads[{}] num_poller_threads[{}] "
          "max_poller_threads[{}] num_resolver_threads[{}]",
          params.num_task_threads, params.max_task_threads,
          params.num_expire_threads, params.max_expire_threads,
          params.num_poller_threads, params.max_poller_threads,
          params.num_resolver_threads);
}

} // namespace z::nng
