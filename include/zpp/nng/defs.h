#pragma once

#include <nng/nng.h>

#ifndef NNG_MAJOR_VERSION
#  error "NNG_MAJOR_VERSION is not defined"
#endif

#ifdef __cplusplus
static_assert(NNG_MAJOR_VERSION == 2, "NNG_MAJOR_VERSION must be 2");
#else
_Static_assert(NNG_MAJOR_VERSION == 2, "NNG_MAJOR_VERSION must be 2");
#endif

#include <zpp/namespace.h>

#ifndef NSB_NNG
#define NSB_NNG NSB_ZPP namespace nng {
#define NSE_NNG } NSE_ZPP
#define USE_NNG using namespace z::nng;
#endif

NSB_NNG
static inline const char* strerr(int err){
    return nng_strerror((nng_err)err);
}
/**
 * @brief The memory will be 64-bit aligned.
 */
static inline void* alloc(size_t size){
    return nng_alloc(size);
}

static inline void free(void* ptr, size_t size){
    nng_free(ptr, size);
}

static inline char* strdup(const char* str){
    return nng_strdup(str);
}

static inline void strfree(char* str){
    nng_strfree(str);
}

static inline void sleep(nng_duration dur){
    nng_msleep(dur);
}

NSE_NNG