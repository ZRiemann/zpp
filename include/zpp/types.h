#pragma once

#include "namespace.h"
#include <cstdint>
#include <cstddef>

NSB_ZPP
/// Alias for numeric error codes returned by zpp APIs.
using err_t = int;

/// Unsigned duration type used for time intervals (nanoseconds by default).
using duration_t = uint64_t;

/// Representation of the CPU time-stamp counter (RDTSC) value.
using tsc_t = uint64_t;
NSE_ZPP