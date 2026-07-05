#pragma once

#include <string>
#include <vector>

#include <zpp/error.h>
#include <zpp/namespace.h>
#include <zpp/nng/aio.h>
#include <zpp/nng/endpoint.h>
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

  /// Starts the raw device with one ingress dial endpoint and one or more
  /// egress listeners.
  z::err_t start(std::string name, const endpoint &ingress,
                 const std::vector<endpoint> &egress,
                 const socket_options &options);
  /// Stops sockets and AIO.
  void stop();

private:
  static void on_event(void *arg) noexcept;

  std::string name_;
  socket front_;
  socket back_;
  aio aio_;
};

} // namespace nng
NSE_ZPP
