#include <zpp/error.h>
#include <zpp/json.hpp>
#include <zpp/nng/aio.h>
#include <zpp/nng/engine.h>
#include <zpp/nng/log.h>
#include <zpp/nng/util.h>

USE_ZPP
USE_NNG

engine::engine(int argc, char **argv) : server(argc, argv) {
  if (argc < 3) {
    nng2war(cmp_runtime, act_configure,
            "usage: {} <uni-conf.json> <conf_svr_name>", argv[0]);
    nng2war(cmp_runtime, act_configure,
            "example: {} ../doc/config/server.json server", argv[0]);
    return;
  }
  json doc;
  doc.load_file(argv_[1]);
  json_view conf;
  json_view conf_engine;
  if (ERR_OK == doc.member(argv_[2], conf) &&
      ERR_OK == conf.member("nng_engine", conf_engine)) {
    rapidjson::StringBuffer sbuf;
    nng2inf(cmp_runtime, act_configure, "nng_engine: {}",
            conf_engine.to_string(sbuf, true));
    // fill nng_init_params from configuration (defaults to 0 == use library
    // defaults)
    nng_init_params params = {0};
    int tmp = 0;

    if (conf_engine.get("task_threads", tmp) == ERR_OK) {
      params.num_task_threads = (int16_t)tmp;
    }
    if (conf_engine.get("task_threads_max", tmp) == ERR_OK) {
      params.max_task_threads = (int16_t)tmp;
    }
    if (conf_engine.get("expire_threads", tmp) == ERR_OK) {
      params.num_expire_threads = (int16_t)tmp;
    }
    if (conf_engine.get("expire_threads_max", tmp) == ERR_OK) {
      params.max_expire_threads = (int16_t)tmp;
    }
    if (conf_engine.get("poller_threads", tmp) == ERR_OK) {
      params.num_poller_threads = (int16_t)tmp;
    }
    if (conf_engine.get("poller_threads_max", tmp) == ERR_OK) {
      params.max_poller_threads = (int16_t)tmp;
    }
    if (conf_engine.get("resolver_threads", tmp) == ERR_OK) {
      params.num_resolver_threads = (int16_t)tmp;
    }

    nng_.emplace(params);
  } else {
    nng2war(cmp_runtime, act_configure,
            "Waring: NOT find spd config item:{}.nng_engine", argv_[2]);
  }
}

engine::~engine() = default;

err_t engine::on_timer() { return ERR_OK; }

err_t engine::timer() {
  z::nng::sleep(1000);
  return ERR_OK;
}
