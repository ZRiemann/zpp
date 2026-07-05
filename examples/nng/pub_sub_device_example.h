#pragma once

#include <vector>

#include <zpp/nng/endpoint.h>
#include <zpp/nng/engine.h>
#include <zpp/nng/protocols/pub_sub_device.h>
#include <zpp/nng/socket.h>

NSB_APP

/// Example application forwarding counters through a raw PUB/SUB device.
class pub_sub_device_example : public nng::engine {
public:
  /// Creates the PUB/SUB device example application.
  pub_sub_device_example(int argc, char **argv);

  /// Loads the shared PUB/SUB topology configuration.
  err_t configure() override;
  /// Starts the asynchronous PUB/SUB device.
  err_t run() override;
  /// Stops the device and closes its endpoints.
  err_t stop() override;

private:
  nng::endpoint ingress_;
  std::vector<nng::endpoint> egress_;
  nng::socket_options socket_options_;
  nng::pub_sub_device device_;
};

NSE_APP
