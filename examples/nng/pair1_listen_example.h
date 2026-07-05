#pragma once

#include "pair1_example.h"

NSB_APP

/// PAIR1 listener example that initiates the ping-pong exchange.
class pair1_listen_example final : public pair1_example {
public:
  /// Creates the PAIR1 listener example.
  pair1_listen_example(int argc, char **argv)
      : pair1_example(argc, argv, nng::endpoint_role::listen, true) {}
};

NSE_APP
