#pragma once

#include <cstdint>

#include <zpp/error.h>
#include <zpp/namespace.h>
#include <zpp/nng/message.h>
#include <zpp/nng/protocols/publisher.h>
#include <zpp/nng/task_timer.h>
#include <zpp/spdlog.h>
#include <zpp/types.h>

NSB_APP

/// Periodically publishes an incrementing message counter.
class msg_timer : public nng::task_timer {
public:
  /// Creates a publisher-backed message timer.
  msg_timer(nng_duration duration, nng::publisher &publisher)
      : nng::task_timer(duration), publisher_(publisher) {}

private:
  /// Publishes one counter value for each timer expiration.
  err_t handle(nng_err /*err*/) override {
    nng::message msg{(size_t)0};
    msg.reserve(256);
    msg.append_u32(static_cast<std::uint32_t>(count_));
    if (publisher_.send(msg) != NNG_OK) {
      spd_inf("failed to send message: {}", count_);
    } else {
      spd_inf("sent message: {}", count_);
    }
    ++count_;
    return ERR_OK;
  }

  std::size_t count_{0};
  nng::publisher &publisher_;
};

NSE_APP
