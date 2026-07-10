#pragma once

#include <string>
#include <vector>

#include <zpp/error.h>
#include <zpp/namespace.h>
#include <zpp/nng/aio.h>
#include <zpp/nng/dialer.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/listener.h>
#include <zpp/nng/socket.h>

NSB_ZPP
namespace nng {

/// NNG raw SUB-to-PUB device.
class pub_sub_device {
public:
  /// Creates an empty PUB/SUB device.
  pub_sub_device() = default;
  /// Stops the device.
  ~pub_sub_device();

  pub_sub_device(const pub_sub_device &) = delete;
  pub_sub_device &operator=(const pub_sub_device &) = delete;

  /**
   * @brief Opens raw PUB/SUB device sockets and prepares ingress and egress
   * endpoints.
   * @param name Stable name used in diagnostic logs.
   * @param ingress Endpoints attached to the raw SUB front socket.
   * @param egress Endpoints attached to the raw PUB back socket.
   * @param socket_config Optional common socket configuration applied to both
   * raw sockets.
   * @param transport_config Optional TCP transport configuration applied to
   * TCP endpoints.
   * @param dialer_config Optional configuration applied to dial endpoints.
   * @param listener_config Optional configuration applied to listen endpoints.
   * @return ERR_OK on success; otherwise ERR_FAIL.
   */
  z::err_t configure(std::string name, std::vector<endpoint> &&ingress,
                     std::vector<endpoint> &&egress,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);
  /// Starts prepared endpoints and the asynchronous raw device.
  z::err_t start();
  /// Stops sockets and AIO.
  void stop();
  /// Returns the endpoints attached to the raw SUB front socket.
  const std::vector<endpoint> &ingress_endpoints() const noexcept;
  /// Returns the endpoints attached to the raw PUB back socket.
  const std::vector<endpoint> &egress_endpoints() const noexcept;

private:
  static void on_event(void *arg) noexcept;

  std::string name_;
  socket front_;
  socket back_;
  aio aio_;
  std::vector<endpoint> ingress_endpoints_;
  std::vector<endpoint> egress_endpoints_;
  std::vector<listener> ingress_listeners_;
  std::vector<dialer> ingress_dialers_;
  std::vector<listener> egress_listeners_;
  std::vector<dialer> egress_dialers_;
};

} // namespace nng
NSE_ZPP
