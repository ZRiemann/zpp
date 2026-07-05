#include "subscriber_example.h"

#include <cstdint>
#include <utility>

#include <zpp/error.h>
#include <zpp/spdlog.h>

#include "protocol_example_common.h"

USE_ZPP

namespace z::app {

subscriber_example::subscriber_example(int argc, char **argv)
    : nng::engine(argc, argv) {}

void counter_subscriber::on_receive(nng::message &msg,
                                    nng_err result) noexcept {
  if (result == NNG_OK) {
    const std::size_t size = msg.valid() ? msg.length() : 0;
    std::uint32_t count{0};
    if (msg.valid() && msg.chop_u32(&count) == NNG_OK) {
      spd_inf("received subscriber message: {} size: {}", count, size);
    } else {
      spd_err("failed to decode subscriber message: size: {}", size);
    }
  } else if (result != NNG_ETIMEDOUT) {
    spd_err("failed to receive subscriber message: [{}] {}",
            static_cast<int>(result), nng_strerror(result));
  }
}

err_t subscriber_example::configure() {
  if (argc_ < 2) {
    spd_err("missing configuration file argument");
    return ERR_FAIL;
  }

  pub_sub_example_config config;
  if (load_pub_sub_example_config(argv_[1], config) != ERR_OK) {
    spd_err("invalid subscriber configuration: {}:nng_net.pub_sub", argv_[1]);
    return ERR_FAIL;
  }
  for (auto &endpoint : config.egress) {
    endpoint.role = nng::endpoint_role::dial;
  }
  return subscriber_.configure(std::move(config.egress), config.topics, nullptr,
                               config.socket ? &*config.socket : nullptr,
                               config.transport ? &*config.transport : nullptr,
                               config.dialer ? &*config.dialer : nullptr,
                               nullptr);
}

err_t subscriber_example::run() {
  if (subscriber_.start(3) != ERR_OK) {
    spd_err("failed to start subscriber");
    return ERR_FAIL;
  }
  return ERR_OK;
}

err_t subscriber_example::stop() {
  subscriber_.stop();
  return ERR_OK;
}

} // namespace z::app

#define SVR_NAME z::app::subscriber_example
#include <zpp/core/main.hpp>
