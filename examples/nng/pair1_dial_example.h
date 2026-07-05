#pragma once

#include "pair1_example.h"

NSB_APP

/// PAIR1 dialer example that replies to listener messages.
class pair1_dial_example final : public pair1_example {
public:
  /// Creates the PAIR1 dialer example.
  pair1_dial_example(int argc, char **argv)
      : pair1_example(argc, argv, nng::endpoint_role::dial, false) {}
};

NSE_APP
