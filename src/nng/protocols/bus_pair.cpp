#include <zpp/nng/protocols/bus.h>
#include <zpp/nng/protocols/pair.h>

#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {

bus::bus() noexcept : endpoint_protocol(cmp_bus) {}

z::err_t bus::configure(std::vector<endpoint> &&endpoints,
                        const socket_options *socket_config,
                        const transport_options *transport_config,
                        const dialer_options *dialer_config,
                        const listener_options *listener_config) {
  return endpoint_protocol::configure(&socket::bus0_open, std::move(endpoints),
                                      socket_config, transport_config,
                                      dialer_config, listener_config);
}

pair::pair() noexcept : endpoint_protocol(cmp_pair) {}

z::err_t pair::configure(endpoint &&target, pair_version version,
                         const socket_options *socket_config,
                         const transport_options *transport_config,
                         const dialer_options *dialer_config,
                         const listener_options *listener_config) {
  std::vector<endpoint> endpoints;
  endpoints.reserve(1);
  endpoints.emplace_back(std::move(target));
  const open_fn open_socket =
      version == pair_version::v0 ? &socket::pair0_open : &socket::pair1_open;
  return endpoint_protocol::configure(open_socket, std::move(endpoints),
                                      socket_config, transport_config,
                                      dialer_config, listener_config);
}

} // namespace z::nng
