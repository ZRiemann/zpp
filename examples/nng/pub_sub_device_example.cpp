#include "pub_sub_device_example.h"

#include <utility>

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
  ingress_ = std::move(config.ingress);
  egress_ = std::move(config.egress);
  if (config.socket) {
    socket_options_ = *config.socket;
  }
  return ERR_OK;
}

err_t pub_sub_device_example::run() {
  return device_.start("pub_sub_device_example", ingress_, egress_,
                       socket_options_);
}

err_t pub_sub_device_example::stop() {
  device_.stop();
  return ERR_OK;
}

} // namespace z::app

#define SVR_NAME z::app::pub_sub_device_example
#include <zpp/core/main.hpp>
