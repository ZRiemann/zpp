#pragma once

#include "defs.h"

#include <optional>

#include <zpp/core/server.h>
#include <zpp/namespace.h>
#include <zpp/nng/nng.h>

NSB_NNG

/// Server base that owns the configured global NNG runtime lifecycle.
class engine : public server {
public:
  /// Creates an engine and initializes NNG from the configured nng_engine
  /// section.
  engine(int argc, char **argv);
  /// Finalizes the configured NNG runtime lifecycle.
  ~engine() override;

  /// Handles one engine timer tick.
  err_t on_timer() override;
  /// Sleeps using the NNG clock helper.
  err_t timer() override;

private:
  std::optional<nng> nng_;
};
NSE_NNG
