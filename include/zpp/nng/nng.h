#pragma once

#include "defs.h"

#include <cstdint>

NSB_NNG

/// RAII owner for the global NNG runtime lifecycle.
class nng {
public:
  /// Initializes NNG with library default thread settings.
  nng();

  /// Initializes NNG with explicit initialization parameters.
  explicit nng(const nng_init_params &params);

  /// Initializes NNG from individual thread settings.
  nng(std::int16_t num_task_thrs, std::int16_t max_task_thrs,
      std::int16_t num_expire_thrs, std::int16_t max_expire_thrs,
      std::int16_t num_poller_thrs, std::int16_t max_poller_thrs,
      std::int16_t num_resolver_thrs);

  /// Finalizes the NNG runtime.
  ~nng();

  nng(const nng &) = delete;
  nng &operator=(const nng &) = delete;
  nng(nng &&) = delete;
  nng &operator=(nng &&) = delete;

private:
  void init(const nng_init_params &params);
};

NSE_NNG
