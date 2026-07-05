#pragma once

#include <nng/nng.h>

#include <zpp/nng/protocols/protocol.h>

NSB_NNG

/// Multi-endpoint PUB0 protocol wrapper.
class publisher : private endpoint_protocol {
public:
  publisher() noexcept;

  z::err_t configure(std::vector<endpoint> &&endpoints,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);

  using endpoint_protocol::endpoints;
  using endpoint_protocol::start;
  using endpoint_protocol::stop;

  /// Sends and consumes a raw message; failures are also consumed and freed.
  int send(nng_msg *msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    const int result = nng_sendmsg(protocol_socket().get(), msg, flags);
    if (result != NNG_OK) {
      nng_msg_free(msg);
    }
    return result;
  }

  int send(message &msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    return endpoint_protocol::send(msg, flags);
  }

  void send(aio &io) noexcept { endpoint_protocol::send(io); }
};

NSE_NNG
