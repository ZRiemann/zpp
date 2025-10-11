#pragma once

/**
 * @file aligned_malloc.hpp
 * @brief Aligned memory allocation and deallocation functions
 * @note This file provides cross-platform functions for allocating and freeing
 *      Copy from folly/Memory.h
 */
#include <zpp/namespace.h>
#include <zpp/system/os.h>
NSB_ZPP

#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600) ||         \
    (defined(__ANDROID__) && (__ANDROID_API__ > 16)) ||         \
    (defined(__APPLE__)) || defined(__FreeBSD__) || defined(__wasm32__)

inline void* aligned_malloc(size_t size, size_t align) {
  // use posix_memalign, but mimic the behaviour of memalign
  void* ptr = nullptr;
  int rc = posix_memalign(&ptr, align, size);
  return rc == 0 ? (errno = 0, ptr) : (errno = rc, nullptr);
}

inline void aligned_free(void* aligned_ptr) {
  free(aligned_ptr);
}

#elif defined(_WIN32)

inline void* aligned_malloc(size_t size, size_t align) {
  return _aligned_malloc(size, align);
}

inline void aligned_free(void* aligned_ptr) {
  _aligned_free(aligned_ptr);
}

#else

inline void* aligned_malloc(size_t size, size_t align) {
  return memalign(align, size);
}

inline void aligned_free(void* aligned_ptr) {
  free(aligned_ptr);
}

#endif


NSE_ZPP