#include <zpp/nng/protocols/puller.h>
#include <zpp/nng/protocols/pusher.h>

#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {

pusher::pusher() noexcept : endpoint_protocol(cmp_pusher) {}

z::err_t pusher::configure(std::vector<endpoint> &&endpoints,
                           const socket_options *socket_config,
                           const transport_options *transport_config,
                           const dialer_options *dialer_config,
                           const listener_options *listener_config) {
  return endpoint_protocol::configure(&socket::push0_open, std::move(endpoints),
                                      socket_config, transport_config,
                                      dialer_config, listener_config);
}

puller::puller() noexcept : endpoint_protocol(cmp_puller) {}

z::err_t puller::configure(std::vector<endpoint> &&endpoints,
                           const socket_options *socket_config,
                           const transport_options *transport_config,
                           const dialer_options *dialer_config,
                           const listener_options *listener_config) {
  return endpoint_protocol::configure(&socket::pull0_open, std::move(endpoints),
                                      socket_config, transport_config,
                                      dialer_config, listener_config);
}

} // namespace z::nng
