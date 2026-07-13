/**
 * @file zpp/core/server.cpp
 */
#include <csignal>
#include <iostream>
#include <string>

#include <zpp/core/monitor.h>
#include <zpp/core/server.h>
#include <zpp/json.hpp>
#include <zpp/spdlog.h>
#include <zpp/system/system.h>
#include <zpp/system/tsc.h>

NSB_STATIC
volatile std::sig_atomic_t s_signal_status{0};

void signal_handler(int signal) { s_signal_status = signal; }

void new_failure_handler() {
  spd_err("Memory INSUFFICIENT!");
  std::raise(SIGINT);
}

NSE_STATIC

USE_ZPP

namespace {

bool parse_log_level(const std::string &value,
                     spdlog::level::level_enum &out) {
  if (value == "trace") {
    out = spdlog::level::trace;
  } else if (value == "debug") {
    out = spdlog::level::debug;
  } else if (value == "info") {
    out = spdlog::level::info;
  } else if (value == "warn" || value == "warning") {
    out = spdlog::level::warn;
  } else if (value == "err" || value == "error") {
    out = spdlog::level::err;
  } else if (value == "critical") {
    out = spdlog::level::critical;
  } else if (value == "off") {
    out = spdlog::level::off;
  } else {
    return false;
  }
  return true;
}

void read_log_level(json_view &conf, const char *key,
                    spdlog::level::level_enum &out) {
  std::string value;
  if (ERR_OK == conf.get(key, value)) {
    parse_log_level(value, out);
  }
}

void read_sink_format(json_view &conf, const char *key,
                      z::log::sink_format &out) {
  std::string value;
  if (ERR_OK != conf.get(key, value)) {
    return;
  }
  if (value == "jsonl") {
    out = z::log::sink_format::jsonl;
  } else if (value == "pattern") {
    out = z::log::sink_format::pattern;
  }
}

void read_positive_size(json_view &conf, const char *key, std::size_t &out) {
  int value = 0;
  if (ERR_OK == conf.get(key, value) && value > 0) {
    out = static_cast<std::size_t>(value);
  }
}

void read_positive_seconds(json_view &conf, const char *key,
                           std::chrono::seconds &out) {
  int value = 0;
  if (ERR_OK == conf.get(key, value) && value > 0) {
    out = std::chrono::seconds(value);
  }
}

void read_file_sink(json_view &conf, z::log::file_sink_config &out) {
  conf.get("enabled", out.enabled);
  conf.get("name", out.file_name);
  conf.get("rotate_on_open", out.rotate_on_open);
  conf.get("pattern", out.pattern);
  read_positive_size(conf, "rotats", out.max_files);
  read_positive_size(conf, "max_size", out.max_file_size_mb);
  read_log_level(conf, "level", out.level);
  read_sink_format(conf, "format", out.format);
}

} // namespace

server::server() {
  tsc_init();

  z::log::config log_conf;
  log_conf.async = false;
  log_conf.console = true;
  log_conf.rotating_file = false;
  z::log::init(log_conf);
}

server::server(int argc, char **argv) : argc_(argc), argv_(argv) {
  if (argc < 3) {
    std::cout << "usage: " << argv[0] << " <uni-conf.json> <conf_svr_name>"
              << std::endl
              << "example: " << argv[0] << " ../doc/config/server.json server"
              << std::endl;
    return;
  }
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << std::endl;

  tsc_init();

  std::set_new_handler(new_failure_handler);
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGFPE, signal_handler);
  std::signal(SIGABRT, signal_handler);
  std::signal(SIGILL, signal_handler);
  std::signal(SIGSEGV, signal_handler);

  json conf;
  if (ERR_OK != conf.load_file(argv[1])) {
    std::cout << "loading config file: " << argv[1] << " FAILED!" << std::endl;
    exit(-1);
  } else {
    std::cout << "loading config file: " << argv[1] << " Success." << std::endl;
    z::log::config log_conf;
    log_conf.rotating_file = true;
    json_view svr_conf;
    json_view spd_conf;
    if (ERR_OK == conf.member(argv[2], svr_conf) &&
        ERR_OK == svr_conf.member("spd", spd_conf)) {
      log_conf.service = argv[2];
      spd_conf.get("async", log_conf.async);
      spd_conf.get("console", log_conf.console);
      spd_conf.get("rotate_on_open", log_conf.rotate_on_open);
      spd_conf.get("name", log_conf.file_name);
      spd_conf.get("pattern", log_conf.pattern);
      spd_conf.get("service", log_conf.service);
      spd_conf.get("node", log_conf.node);
      spd_conf.get("run_id", log_conf.run_id);
      spd_conf.get("logger_name", log_conf.logger_name);
      read_log_level(spd_conf, "level", log_conf.level);
      read_log_level(spd_conf, "flush_level", log_conf.flush_level);

      read_positive_size(spd_conf, "rotats", log_conf.max_files);
      read_positive_size(spd_conf, "max_size", log_conf.max_file_size_mb);
      read_positive_size(spd_conf, "queue_size", log_conf.queue_size);
      read_positive_seconds(spd_conf, "flush_interval_sec",
                            log_conf.flush_interval);

      json_view important_conf;
      if (ERR_OK == spd_conf.member("important", important_conf)) {
        read_file_sink(important_conf, log_conf.important_file);
        if (log_conf.important_file.enabled &&
            log_conf.important_file.file_name.empty()) {
          log_conf.important_file.file_name =
              log_conf.service.empty() ? "zpp.important.jsonl"
                                       : log_conf.service + ".important.jsonl";
        }
      }
    } else {
      std::cout << "Waring: NOT find spd config item: " << argv[2] << std::endl;
    }

    z::log::init(log_conf);

    sys::info();
    std::string path, app_name;
    sys::process_path(path, app_name);
    spd_inf("path:{}, name:{};", path, app_name);
    rapidjson::StringBuffer sbuf;
    spd_inf("spd:{}", spd_conf.to_string(sbuf, true));
  }
}

server::~server() {
  monitor_guard::print_statistic();
  z::log::shutdown();
}

#include <iostream>

err_t server::configure() {
  spd_mark();
  return ERR_OK;
}

err_t server::run() {
  spd_mark();
  return ERR_OK;
}

err_t server::stop() {
  spd_mark();
  return ERR_OK;
}

err_t server::wait_stop() {
  spd_mark();
  return ERR_OK;
}

err_t server::on_timer() { return ERR_END; }
err_t server::timer() { return ERR_END; }

err_t server::handle_signal(int signal) {
  return signal != 0 ? ERR_END : ERR_OK;
}

void server::loop() {
  spd_mark();
  for (;;) {
    if (ERR_OK != timer()) {
      spd_inf("timer exit.");
      break;
    }
    if (ERR_OK != handle_signal(s_signal_status)) {
      spd_inf("signal[{}] exit.", s_signal_status);
      break;
    } else if (ERR_OK != on_timer()) {
      spd_inf("on_timer exit.");
      break;
    }
  }
}
