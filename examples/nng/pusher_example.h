#pragma once

#include <zpp/nng/engine.h>
#include <zpp/nng/protocols/pusher.h>

#include "protocol_example_common.h"

NSB_APP

/// Example application periodically sending counters through PUSH0.
class pusher_example : public nng::engine {
public:
  /// Creates the PUSH0 example application.
  pusher_example(int argc, char **argv)
      : nng::engine(argc, argv), sender_(1000, protocol_) {}

  /// Loads the pipeline endpoint configuration.
  err_t configure() override {
    if (argc_ < 2) {
      spd_err("missing configuration file argument");
      return ERR_FAIL;
    }
    protocol_example_config config;
    if (load_protocol_example_config(argv_[1], "pipeline",
                                     nng::endpoint_role::listen,
                                     config) != ERR_OK) {
      spd_err("invalid pusher configuration: {}:nng_net.pipeline", argv_[1]);
      return ERR_FAIL;
    }
    return protocol_.configure(std::move(config.endpoints),
                               config.socket ? &*config.socket : nullptr,
                               config.transport ? &*config.transport : nullptr,
                               config.dialer ? &*config.dialer : nullptr,
                               config.listener ? &*config.listener : nullptr);
  }

  /// Starts the PUSH0 endpoints and periodic sender.
  err_t run() override {
    if (protocol_.start() != ERR_OK) {
      return ERR_FAIL;
    }
    sender_.start();
    return ERR_OK;
  }

  /// Stops periodic sends and closes the protocol.
  err_t stop() override {
    sender_.stop();
    protocol_.stop();
    return ERR_OK;
  }

private:
  nng::pusher protocol_;
  protocol_counter_sender<nng::pusher> sender_;
};

NSE_APP
