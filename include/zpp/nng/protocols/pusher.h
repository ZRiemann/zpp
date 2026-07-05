#pragma once

#include <zpp/nng/protocols/protocol.h>

NSB_NNG

/// Multi-endpoint PUSH0 protocol wrapper.
class pusher : private endpoint_protocol {
public:
  pusher() noexcept;
  z::err_t configure(std::vector<endpoint> &&endpoints,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);
  using endpoint_protocol::endpoints;
  using endpoint_protocol::start;
  using endpoint_protocol::stop;
  int send(message &msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    return endpoint_protocol::send(msg, flags);
  }
  void send(aio &io) noexcept { endpoint_protocol::send(io); }
};

NSE_NNG
