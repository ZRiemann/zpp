#pragma once

#include <zpp/nng/protocols/protocol.h>

NSB_NNG

struct surveyor_options {
  /// Time during which responses are accepted for a survey.
  nng_duration survey_time_ms{1000};
};

/// Multi-endpoint SURVEYOR0 protocol wrapper.
class surveyor : private endpoint_protocol {
public:
  surveyor() noexcept;
  z::err_t configure(std::vector<endpoint> &&endpoints,
                     const surveyor_options *surveyor_config = nullptr,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);
  using endpoint_protocol::endpoints;
  using endpoint_protocol::start;
  using endpoint_protocol::stop;
  int open_context(aio_ctx &context, aio_ctx::callback callback = nullptr,
                   void *arg = nullptr,
                   const surveyor_options *options = nullptr) const noexcept;
  int send(message &msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    return endpoint_protocol::send(msg, flags);
  }
  void send(aio &io) noexcept { endpoint_protocol::send(io); }
  int recv(message &msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    return endpoint_protocol::recv(msg, flags);
  }
  void recv(aio &io) noexcept { endpoint_protocol::recv(io); }

private:
  static int configure_socket(socket &target, const void *data);
};

NSE_NNG
