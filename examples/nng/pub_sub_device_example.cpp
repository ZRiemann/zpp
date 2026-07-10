#include "pub_sub_device_example.h"

#include <utility>
#include <vector>

#include <zpp/error.h>
#include <zpp/spdlog.h>

#include "protocol_example_common.h"

USE_ZPP

namespace z::app {

pub_sub_device_example::pub_sub_device_example(int argc, char **argv)
    : nng::engine(argc, argv) {}

err_t pub_sub_device_example::configure() {
  if (argc_ < 2) {
    spd_err("missing configuration file argument");
    return ERR_FAIL;
  }

  pub_sub_example_config config;
  if (load_pub_sub_example_config(argv_[1], config) != ERR_OK) {
    spd_err("invalid PUB/SUB device configuration: {}:nng_net.pub_sub",
            argv_[1]);
    return ERR_FAIL;
  }
  std::vector<nng::endpoint> ingress;
  ingress.push_back(std::move(config.ingress));
  return device_.configure(
      "pub_sub_device_example", std::move(ingress), std::move(config.egress),
      config.socket ? &*config.socket : nullptr,
      config.transport ? &*config.transport : nullptr,
      config.dialer ? &*config.dialer : nullptr,
      config.listener ? &*config.listener : nullptr);
}

err_t pub_sub_device_example::run() {
  return device_.start();
}

err_t pub_sub_device_example::stop() {
  device_.stop();
  return ERR_OK;
}

} // namespace z::app

#define SVR_NAME z::app::pub_sub_device_example
#include <zpp/core/main.hpp>
