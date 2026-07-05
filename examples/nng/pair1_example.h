#pragma once

#include <cstdint>

#include <zpp/nng/engine.h>
#include <zpp/nng/protocols/pair.h>

#include "protocol_example_common.h"

NSB_APP

/// Shared asynchronous PAIR1 ping-pong example implementation.
class pair1_example : public nng::engine {
public:
  /// Creates one PAIR1 endpoint with a fixed local role.
  pair1_example(int argc, char **argv, nng::endpoint_role role,
                bool sends_first)
      : nng::engine(argc, argv), role_(role), sends_first_(sends_first) {}

  /// Loads the single PAIR1 endpoint configuration.
  err_t configure() override {
    if (argc_ < 2) {
      spd_err("missing configuration file argument");
      return ERR_FAIL;
    }
    protocol_example_config config;
    if (load_protocol_example_config(argv_[1], "pair1", role_, config) !=
            ERR_OK ||
        config.endpoints.size() != 1) {
      spd_err("invalid PAIR1 configuration: {}:nng_net.pair1", argv_[1]);
      return ERR_FAIL;
    }
    return protocol_.configure(std::move(config.endpoints.front()),
                               nng::pair_version::v1,
                               config.socket ? &*config.socket : nullptr,
                               config.transport ? &*config.transport : nullptr,
                               config.dialer ? &*config.dialer : nullptr,
                               config.listener ? &*config.listener : nullptr);
  }

  /// Starts the PAIR1 endpoint and ping-pong state machine.
  err_t run() override {
    if (protocol_.start() != ERR_OK ||
        io_.create(&pair1_example::complete, this) != NNG_OK) {
      protocol_.stop();
      return ERR_FAIL;
    }
    if (sends_first_) {
      send_counter();
    } else {
      receive_next();
    }
    return ERR_OK;
  }

  /// Stops asynchronous work and closes the PAIR1 endpoint.
  err_t stop() override {
    if (io_) {
      io_.stop();
    }
    protocol_.stop();
    return ERR_OK;
  }

private:
  enum class state {
    receive,
    send,
    delay,
  };

  static void complete(void *arg) {
    static_cast<pair1_example *>(arg)->on_complete();
  }

  void receive_next() {
    state_ = state::receive;
    protocol_.recv(io_);
  }

  void send_counter() {
    nng_msg *message{nullptr};
    int result = nng_msg_alloc(&message, 0);
    if (result == NNG_OK) {
      result = nng_msg_append_u32(message, counter_);
    }
    if (result != NNG_OK) {
      nng_msg_free(message);
      spd_err("failed to create PAIR1 message: [{}] {}", result,
              nng_strerror(static_cast<nng_err>(result)));
      return;
    }
    state_ = state::send;
    io_.set_msg(message);
    protocol_.send(io_);
  }

  void on_complete() {
    const nng_err result = io_.result();
    if (result == NNG_ESTOPPED || result == NNG_ECANCELED ||
        result == NNG_ECLOSED) {
      return;
    }
    if (result != NNG_OK) {
      if (state_ == state::send) {
        nng_msg_free(io_.get_msg());
        io_.set_msg(nullptr);
        send_counter();
      } else if (state_ == state::delay) {
        io_.sleep(1000);
      } else {
        if (result != NNG_ETIMEDOUT) {
          spd_err("PAIR1 receive failed: [{}] {}", static_cast<int>(result),
                  nng_strerror(result));
        }
        receive_next();
      }
      return;
    }
    if (state_ == state::delay) {
      send_counter();
      return;
    }
    if (state_ == state::send) {
      spd_inf("sent PAIR1 counter: {}", counter_);
      receive_next();
      return;
    }

    nng_msg *message = io_.get_msg();
    std::uint32_t received{0};
    if (message != nullptr && nng_msg_chop_u32(message, &received) == NNG_OK) {
      spd_inf("received PAIR1 counter: {}", received);
      counter_ = received + 1;
    } else {
      spd_err("failed to decode PAIR1 counter");
    }
    nng_msg_free(message);
    io_.set_msg(nullptr);
    state_ = state::delay;
    io_.sleep(1000);
  }

  nng::pair protocol_;
  nng::aio io_;
  nng::endpoint_role role_;
  bool sends_first_{false};
  state state_{state::receive};
  std::uint32_t counter_{0};
};

NSE_APP
