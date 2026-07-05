#pragma once

#include <cstdint>

#include <zpp/nng/engine.h>
#include <zpp/nng/message.h>
#include <zpp/nng/protocols/replier.h>
#include <zpp/spdlog.h>

#include "protocol_example_common.h"

NSB_APP

/**
 * @brief Concurrent REP0 service that echoes request counters.
 */
class replier_app : public nng::replier {
protected:
  /// Decodes a request and starts its reply operation.
  void on_receive(nng::aio_ctx &ctx, nng_err result) noexcept override {
    if (result != NNG_OK) {
      spd_err("receive failed: [{}] {}", static_cast<int>(result),
              nng_strerror(result));
      ctx.recv();
      return;
    }

    nng::message msg{ctx.io().release_msg()};
    std::uint32_t counter{0};
    if (!msg.valid() || msg.chop_u32(&counter) != NNG_OK) {
      spd_err("failed to decode request");
      ctx.recv();
      return;
    }

    spd_inf("received request: {}", counter);
    msg.clear();
    if (msg.append_u32(counter) != NNG_OK) {
      ctx.recv();
      return;
    }
    ctx.send(msg);
  }

  /// Releases a failed reply and starts the next receive operation.
  void on_send(nng::aio_ctx &ctx, nng_err result) noexcept override {
    if (result != NNG_OK) {
      spd_err("send failed: [{}] {}", static_cast<int>(result),
              nng_strerror(result));
      nng::message release{ctx.io().release_msg()};
    }
    ctx.recv();
  }
};

/**
 * @brief Example application serving three concurrent REP0 transactions.
 */
class replier_example : public nng::engine {
public:
  /// Creates the replier example application.
  replier_example(int argc, char **argv) : nng::engine(argc, argv) {}

  /// Loads the request/reply endpoint configuration.
  err_t configure() override {
    if (argc_ < 2) {
      spd_err("missing configuration file argument");
      return ERR_FAIL;
    }
    protocol_example_config config;
    if (load_protocol_example_config(argv_[1], "request_reply",
                                     nng::endpoint_role::listen,
                                     config) != ERR_OK) {
      spd_err("invalid replier configuration: {}:nng_net.request_reply",
              argv_[1]);
      return ERR_FAIL;
    }
    return protocol_.configure(std::move(config.endpoints),
                               config.socket ? &*config.socket : nullptr,
                               config.transport ? &*config.transport : nullptr,
                               config.dialer ? &*config.dialer : nullptr,
                               config.listener ? &*config.listener : nullptr);
  }

  /// Starts the endpoints and three concurrent receive loops.
  err_t run() override { return protocol_.start(3); }

  /// Stops all receive loops and endpoints.
  err_t stop() override {
    protocol_.stop();
    return ERR_OK;
  }

private:
  replier_app protocol_;
};

NSE_APP
