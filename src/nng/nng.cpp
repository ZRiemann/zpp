#include <zpp/nng/nng.h>

#include <zpp/nng/log.h>
#include <zpp/spd_log.h>

#include <cstdlib>
#include <mutex>

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

void zpp_nng_logger(nng_log_level level, nng_log_facility facility,
                    const char *msgid, const char *msg) {
  // spdlog treats line 0 as an empty source location, so use a synthetic
  // line 1.
  nng2log(spdlog::source_loc{"nng-runtime", 1, "nng_logger"},
          map_nng_level_to_spd_level(level), cmp_runtime, act_logger,
          "facility={} msgid={} {}", static_cast<int>(facility),
          msgid == nullptr ? "-" : msgid, msg == nullptr ? "" : msg);
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
