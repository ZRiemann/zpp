#pragma once

#include "defs.h"
#include "log.h"
#include "util.h"

NSB_NNG
class message {
public:
#pragma region Constructors and Destructor
  message() {
    // Used by socket recv message, or by nng_msg_dup;
  }
  explicit message(size_t size) {
    int ret = nng_msg_alloc(&msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_alloc failed: {}",
               strerr(ret));
    }
    // spd_inf("msg_ allocated: {}", (void*)msg_);
  }
  ~message() noexcept {
    if (msg_) {
      // spd_inf("msg_{} freed.", (void*)msg_);
      nng_msg_free(msg_);
    }
  }
  explicit message(const message &duplicate) {
    int ret = nng_msg_dup(&msg_, duplicate.msg_);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_dup failed: {}", strerr(ret));
    }
  }

  explicit message(nng_msg *msg) noexcept : msg_(msg) {}
  /// Releases the owned NNG message and leaves this wrapper empty.
  nng_msg *release() noexcept {
    nng_msg *released = msg_;
    msg_ = nullptr;
    return released;
  }

  bool valid() const noexcept { return msg_ != nullptr; }
#pragma endregion
#pragma region Message Body
  size_t capacity() const { return nng_msg_capacity(msg_); }
  int reallocate(size_t size) {
    int ret = nng_msg_realloc(msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_realloc failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int reserve(size_t size) {
    int ret = nng_msg_reserve(msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_reserve failed: {}",
               strerr(ret));
    }
    return ret;
  }

  size_t length() const { return nng_msg_len(msg_); }
  void *body() const { return nng_msg_body(msg_); }
  /**
   * @brief simply resets the total message body length to zero, but does not
   * affect the capacity.
   * @note It does not change the underlying bytes of the message.
   */
  void clear() { nng_msg_clear(msg_); }
#pragma region Add to Body
  int append(const void *data, size_t size) {
    int ret = nng_msg_append(msg_, data, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_append failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int append_u16(uint16_t v) {
    int ret = nng_msg_append_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_append_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int append_u32(uint32_t v) {
    int ret = nng_msg_append_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_append_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int append_u64(uint64_t v) {
    int ret = nng_msg_append_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_append_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }

  int insert(const void *data, size_t size) {
    int ret = nng_msg_insert(msg_, data, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_insert failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int insert_u16(uint16_t v) {
    int ret = nng_msg_insert_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_insert_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int insert_u32(uint32_t v) {
    int ret = nng_msg_insert_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_insert_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int insert_u64(uint64_t v) {
    int ret = nng_msg_insert_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_insert_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }
#pragma endregion
#pragma region Consume From Body
  int trim(size_t size) {
    int ret = nng_msg_trim(msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_trim failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int trim_u16(uint16_t *v) {
    int ret = nng_msg_trim_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_trim_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int trim_u32(uint32_t *v) {
    int ret = nng_msg_trim_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_trim_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int trim_u64(uint64_t *v) {
    int ret = nng_msg_trim_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_trim_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int chop(size_t size) {
    int ret = nng_msg_chop(msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_chop failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int chop_u16(uint16_t *v) {
    int ret = nng_msg_chop_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_chop_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int chop_u32(uint32_t *v) {
    int ret = nng_msg_chop_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_chop_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int chop_u64(uint64_t *v) {
    int ret = nng_msg_chop_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_chop_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }
#pragma endregion
#pragma endregion
#pragma region Message Header
#pragma region Get or Clear Header
  void *header() const { return nng_msg_header(msg_); }
  size_t header_length() const { return nng_msg_header_len(msg_); }
  void header_clear() { nng_msg_header_clear(msg_); }
#pragma endregion
#pragma region Append or Insert Header
  int header_append(const void *data, size_t size) {
    int ret = nng_msg_header_append(msg_, data, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_append failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_append_u16(uint16_t v) {
    int ret = nng_msg_header_append_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_append_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_append_u32(uint32_t v) {
    int ret = nng_msg_header_append_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_append_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_append_u64(uint64_t v) {
    int ret = nng_msg_header_append_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_append_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }

  int header_insert(const void *data, size_t size) {
    int ret = nng_msg_header_insert(msg_, data, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_insert failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_insert_u16(uint16_t v) {
    int ret = nng_msg_header_insert_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_insert_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_insert_u32(uint32_t v) {
    int ret = nng_msg_header_insert_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_insert_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_insert_u64(uint64_t v) {
    int ret = nng_msg_header_insert_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_insert_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }
#pragma endregion
#pragma region Consume from Header
  int header_trim(size_t size) {
    int ret = nng_msg_header_trim(msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_trim failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_trim_u16(uint16_t *v) {
    int ret = nng_msg_header_trim_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_trim_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_trim_u32(uint32_t *v) {
    int ret = nng_msg_header_trim_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_trim_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_trim_u64(uint64_t *v) {
    int ret = nng_msg_header_trim_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_trim_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_chop(size_t size) {
    int ret = nng_msg_header_chop(msg_, size);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_chop failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_chop_u16(uint16_t *v) {
    int ret = nng_msg_header_chop_u16(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_chop_u16 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_chop_u32(uint32_t *v) {
    int ret = nng_msg_header_chop_u32(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_chop_u32 failed: {}",
               strerr(ret));
    }
    return ret;
  }
  int header_chop_u64(uint64_t *v) {
    int ret = nng_msg_header_chop_u64(msg_, v);
    if (ret != 0) {
      nng2errf(cmp_message, act_message, "nng_msg_header_chop_u64 failed: {}",
               strerr(ret));
    }
    return ret;
  }
#pragma endregion
#pragma endregion
#pragma region Message Pipe
  nng_pipe pipe() const { return nng_msg_get_pipe(msg_); }
  void set_pipe(nng_pipe p) { nng_msg_set_pipe(msg_, p); }
#pragma endregion
public:
  /// Owned NNG message, or nullptr when this wrapper is empty.
  nng_msg *msg_{nullptr};
};
NSE_NNG
