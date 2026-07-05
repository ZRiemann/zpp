#pragma once

#include <zpp/nng/engine.h>
#include <zpp/nng/protocols/publisher.h>

#include "msg_timer.hpp"

NSB_APP

/// Example application publishing periodic counters into a PUB/SUB device.
class publisher_example : public nng::engine {
public:
  /// Creates the publisher example application.
  publisher_example(int argc, char **argv);

  /// Loads and applies the shared PUB/SUB topology configuration.
  err_t configure() override;
  /// Starts publisher endpoints and periodic sends.
  err_t run() override;
  /// Stops periodic sends and publisher resources.
  err_t stop() override;

private:
  nng::publisher publisher_;
  msg_timer timer_;
};

NSE_APP
