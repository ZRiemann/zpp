#include <zpp/nng/protocols/publisher.h>

#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {

publisher::publisher() noexcept : endpoint_protocol(cmp_publisher) {}

z::err_t publisher::configure(std::vector<endpoint> &&endpoints,
                              const socket_options *socket_config,
                              const transport_options *transport_config,
                              const dialer_options *dialer_config,
                              const listener_options *listener_config) {
  return endpoint_protocol::configure(&socket::pub0_open, std::move(endpoints),
                                      socket_config, transport_config,
                                      dialer_config, listener_config);
}

} // namespace z::nng
