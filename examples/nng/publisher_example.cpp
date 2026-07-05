#include "publisher_example.h"

#include <utility>
#include <vector>

#include <zpp/error.h>
#include <zpp/spdlog.h>

#include "protocol_example_common.h"

USE_ZPP

namespace z::app {

publisher_example::publisher_example(int argc, char **argv)
    : nng::engine(argc, argv), timer_(nng_duration{1000}, publisher_) {}

err_t publisher_example::configure() {
  if (argc_ < 2) {
    spd_err("missing configuration file argument");
    return ERR_FAIL;
  }

  pub_sub_example_config config;
  if (load_pub_sub_example_config(argv_[1], config) != ERR_OK) {
    spd_err("invalid publisher configuration: {}:nng_net.pub_sub", argv_[1]);
    return ERR_FAIL;
  }
  config.ingress.role = nng::endpoint_role::listen;
  std::vector<nng::endpoint> endpoints;
  endpoints.push_back(std::move(config.ingress));
  return publisher_.configure(
      std::move(endpoints), config.socket ? &*config.socket : nullptr,
      config.transport ? &*config.transport : nullptr, nullptr,
      config.listener ? &*config.listener : nullptr);
}

err_t publisher_example::run() {
  if (publisher_.start() != ERR_OK) {
    spd_err("failed to start publisher");
    return ERR_FAIL;
  }
  timer_.start();
  return ERR_OK;
}

err_t publisher_example::stop() {
  timer_.stop();
  publisher_.stop();
  return ERR_OK;
}

} // namespace z::app

#define SVR_NAME z::app::publisher_example
#include <zpp/core/main.hpp>
