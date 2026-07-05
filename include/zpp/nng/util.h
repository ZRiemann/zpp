#pragma once

#include "defs.h"

NSB_NNG
static inline const char *strerr(int err) {
  return nng_strerror(static_cast<::nng_err>(err));
}
/**
 * @brief The memory will be 64-bit aligned.
 */
static inline void *alloc(size_t size) { return nng_alloc(size); }

static inline void free(void *ptr, size_t size) { nng_free(ptr, size); }

static inline char *strdup(const char *str) { return nng_strdup(str); }

static inline void strfree(char *str) { nng_strfree(str); }

static inline void sleep(nng_duration dur) { nng_msleep(dur); }

NSE_NNG
