#pragma once

#include <nng/nng.h>

#ifndef NNG_MAJOR_VERSION
#error "NNG_MAJOR_VERSION is not defined"
#endif

#ifdef __cplusplus
static_assert(NNG_MAJOR_VERSION == 2, "NNG_MAJOR_VERSION must be 2");
#else
_Static_assert(NNG_MAJOR_VERSION == 2, "NNG_MAJOR_VERSION must be 2");
#endif

#include <zpp/namespace.h>

#ifndef NSB_NNG
#define NSB_NNG NSB_ZPP namespace nng {
#define NSE_NNG                                                                \
  }                                                                            \
  NSE_ZPP
#define USE_NNG using namespace z::nng;
#endif

NSB_NNG
/// Transport-specific options shared by NNG listeners and dialers.
struct transport_options {
  /// Enable TCP_NODELAY when supported.
  bool tcp_no_delay{true};
  /// Enable TCP_KEEPALIVE when supported.
  bool tcp_keepalive{true};
};
NSE_NNG
