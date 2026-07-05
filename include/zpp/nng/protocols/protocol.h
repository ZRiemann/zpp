#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include <zpp/error.h>
#include <zpp/nng/aio_ctx.h>
#include <zpp/nng/dialer.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/listener.h>
#include <zpp/nng/socket.h>

NSB_NNG

/**
 * @brief Common lifecycle for protocols that support multiple endpoints.
 * @details Concrete protocols expose only their valid send and receive
 * directions.
 */
class endpoint_protocol {
public:
  endpoint_protocol(const endpoint_protocol &) = delete;
  endpoint_protocol &operator=(const endpoint_protocol &) = delete;
  endpoint_protocol(endpoint_protocol &&) = delete;
  endpoint_protocol &operator=(endpoint_protocol &&) = delete;

  /// Starts listeners first and dialers second.
  z::err_t start();

  /// Stops the socket and discards all endpoint configuration.
  /// @warning Every context opened from this protocol must be closed first.
  void stop();

  /// Returns the configured endpoints.
  const std::vector<endpoint> &endpoints() const noexcept;

protected:
  using open_fn = int (socket::*)(bool);
  using configure_fn = int (*)(socket &, const void *);

  explicit endpoint_protocol(std::string_view component) noexcept;
  virtual ~endpoint_protocol() = default;

  z::err_t configure(open_fn open_socket, std::vector<endpoint> &&endpoints,
                     const socket_options *socket_config,
                     const transport_options *transport_config,
                     const dialer_options *dialer_config,
                     const listener_options *listener_config,
                     configure_fn protocol_configure = nullptr,
                     const void *protocol_config = nullptr);

  int open_context(aio_ctx &context, aio_ctx::callback callback,
                   void *arg) const noexcept;

  int send(message &msg, int flags) noexcept;
  void send(aio &io) noexcept;
  int recv(message &msg, int flags) noexcept;
  void recv(aio &io) noexcept;

  /// Normalizes a managed asynchronous concurrency count.
  static std::size_t normalize_concurrency(std::size_t requested) noexcept;

  socket &protocol_socket() noexcept;
  const socket &protocol_socket() const noexcept;

protected:
  z::err_t configure_endpoints(const std::vector<endpoint> &endpoints,
                               const transport_options *transport_config,
                               const dialer_options *dialer_config,
                               const listener_options *listener_config);
  z::err_t start_endpoints();

  std::string_view component_;
  socket socket_;
  std::vector<endpoint> endpoints_;
  std::vector<listener> listeners_;
  std::vector<dialer> dialers_;
};

NSE_NNG
