#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <zpp/nng/engine.h>
#include <zpp/nng/message.h>
#include <zpp/nng/protocols/requester.h>
#include <zpp/spdlog.h>

#include "protocol_example_common.h"

NSB_APP

/**
 * @brief Concurrent requester that correlates out-of-order replies with stable
 * hints.
 */
class requester_app : public nng::requester {
public:
  /// Number of concurrent request slots used by the example.
  static constexpr std::size_t concurrency{3};

  /// Submits one request for every currently idle slot.
  void submit_available() noexcept {
    for (auto &slot : slots_) {
      bool expected{false};
      if (!slot.in_flight.compare_exchange_strong(expected, true)) {
        continue;
      }

      slot.counter = next_counter_++;
      nng::message msg{std::size_t{0}};
      if (!msg.valid() || msg.append_u32(slot.counter) != NNG_OK) {
        slot.in_flight.store(false, std::memory_order_release);
        continue;
      }

      const int result = request(msg, &slot);
      if (result != NNG_OK) {
        slot.in_flight.store(false, std::memory_order_release);
        if (result != NNG_EAGAIN) {
          spd_err("failed to submit request {}: [{}] {}", slot.counter, result,
                  nng_strerror(static_cast<nng_err>(result)));
        }
      } else {
        spd_inf("submitted request: {}", slot.counter);
      }
    }
  }

  /**
   * @brief Validates one reply and releases its correlation slot.
   */
  void on_reply(nng::message &msg, nng_err result,
                void *hint) noexcept override {
    auto *slot = static_cast<request_slot *>(hint);
    if (slot == nullptr) {
      spd_err("request completion is missing its correlation hint");
      return;
    }

    std::uint32_t response{0};
    if (result == NNG_OK && msg.valid() && msg.chop_u32(&response) == NNG_OK) {
      if (response == slot->counter) {
        spd_inf("received reply: {}", response);
      } else {
        spd_err("reply mismatch: expected {} received {}", slot->counter,
                response);
      }
    } else {
      spd_err("request {} failed: [{}] {}", slot->counter,
              static_cast<int>(result), nng_strerror(result));
    }
    slot->in_flight.store(false, std::memory_order_release);
  }

private:
  /// Stable per-request correlation data passed through requester::request().
  struct request_slot {
    std::atomic_bool in_flight{false};
    std::uint32_t counter{0};
  };

  std::array<request_slot, concurrency> slots_{};
  std::uint32_t next_counter_{0};
};

/**
 * @brief Example application issuing three concurrent asynchronous REQ0
 * requests.
 */
class requester_example : public nng::engine {
public:
  /// Creates the requester example application.
  requester_example(int argc, char **argv) : nng::engine(argc, argv) {}

  /// Loads the request/reply endpoint configuration.
  err_t configure() override {
    if (argc_ < 2) {
      spd_err("missing configuration file argument");
      return ERR_FAIL;
    }
    protocol_example_config config;
    if (load_protocol_example_config(argv_[1], "request_reply",
                                     nng::endpoint_role::dial,
                                     config) != ERR_OK) {
      spd_err("invalid requester configuration: {}:nng_net.request_reply",
              argv_[1]);
      return ERR_FAIL;
    }
    return protocol_.configure(std::move(config.endpoints), nullptr,
                               config.socket ? &*config.socket : nullptr,
                               config.transport ? &*config.transport : nullptr,
                               config.dialer ? &*config.dialer : nullptr,
                               config.listener ? &*config.listener : nullptr);
  }

  /// Starts the endpoints and the three-context request pool.
  err_t run() override {
    const err_t result = protocol_.start(requester_app::concurrency);
    if (result == ERR_OK) {
      protocol_.submit_available();
    }
    return result;
  }

  /// Submits a new request for each slot completed since the previous timer
  /// tick.
  err_t on_timer() override {
    protocol_.submit_available();
    return ERR_OK;
  }

  /// Stops asynchronous requests and their endpoints.
  err_t stop() override {
    protocol_.stop();
    return ERR_OK;
  }

private:
  requester_app protocol_;
};

NSE_APP
