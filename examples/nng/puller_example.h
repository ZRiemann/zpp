#pragma once

#include <zpp/nng/engine.h>
#include <zpp/nng/protocols/puller.h>

#include "protocol_example_common.h"

NSB_APP

/// Example application asynchronously receiving counters through PULL0.
class puller_example : public nng::engine {
public:
  /// Creates the PULL0 example application.
  puller_example(int argc, char **argv)
      : nng::engine(argc, argv), receiver_(protocol_) {}

  /// Loads the pipeline endpoint configuration.
  err_t configure() override {
    if (argc_ < 2) {
      spd_err("missing configuration file argument");
      return ERR_FAIL;
    }
    protocol_example_config config;
    if (load_protocol_example_config(
            argv_[1], "pipeline", nng::endpoint_role::dial, config) != ERR_OK) {
      spd_err("invalid puller configuration: {}:nng_net.pipeline", argv_[1]);
      return ERR_FAIL;
    }
    return protocol_.configure(std::move(config.endpoints),
                               config.socket ? &*config.socket : nullptr,
                               config.transport ? &*config.transport : nullptr,
                               config.dialer ? &*config.dialer : nullptr,
                               config.listener ? &*config.listener : nullptr);
  }

  /// Starts the PULL0 endpoints and asynchronous receive loop.
  err_t run() override {
    if (protocol_.start() != ERR_OK) {
      return ERR_FAIL;
    }
    if (receiver_.start() != NNG_OK) {
      protocol_.stop();
      return ERR_FAIL;
    }
    return ERR_OK;
  }

  /// Stops asynchronous receives and closes the protocol.
  err_t stop() override {
    receiver_.stop();
    protocol_.stop();
    return ERR_OK;
  }

private:
  nng::puller protocol_;
  protocol_counter_receiver<nng::puller> receiver_;
};

NSE_APP
