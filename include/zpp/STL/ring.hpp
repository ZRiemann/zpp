#pragma once

#include <atomic>
#include <cstdlib>
#include <zpp/core/memory/aligned_malloc.hpp>
#include <zpp/error.h>
#include <zpp/namespace.h>
#include <zpp/system/spin.h>
#include <zpp/types.h>

NSB_ZPP

template <typename T> class ring {
public:
  ring(ring &&other) = delete;
  ring(ring &other) = delete;

  explicit ring(std::size_t capacity, std::size_t publish_batch = 1)
      : capacity_(round_up_pow2(capacity > 1 ? capacity : 2)),
        mask_(capacity_ - 1),
        publish_batch_(publish_batch > 0 ? publish_batch : 1),
        buffer_(static_cast<T *>(aligned_malloc(sizeof(T) * capacity_, 64))),
        write_local_(0), write_cached_(0), write_pub_(0), unpublished_push_(0),
        read_local_(0), read_cached_(0), read_pub_(0), need_publish_(false) {}

  ~ring() { aligned_free(buffer_); }

  std::size_t capacity() const noexcept { return capacity_; }

  std::size_t approx_size() const noexcept {
    const std::size_t write = write_pub_.load(std::memory_order_acquire);
    const std::size_t read = read_pub_.load(std::memory_order_acquire);
    return write - read;
  }

public: // Consume
  inline bool empty() const noexcept {
    if (read_local_ != write_cached_) {
      return false;
    }
    need_publish_.store(true, std::memory_order_relaxed);
    write_cached_ = write_pub_.load(std::memory_order_acquire);
    return read_local_ == write_cached_;
  }

  inline T &front() noexcept { return buffer_[read_local_ & mask_]; }

  inline bool pop() noexcept {
    if (empty()) {
      return false;
    }
    ++read_local_;
    read_pub_.store(read_local_, std::memory_order_release);
    return true;
  }

  inline bool pop(T &t) noexcept {
    if (empty()) {
      return false;
    }
    t = front();
    ++read_local_;
    read_pub_.store(read_local_, std::memory_order_release);
    return true;
  }

public: // Produce
  inline bool full() const noexcept {
    if ((write_local_ - read_cached_) < capacity_) {
      return false;
    }
    read_cached_ = read_pub_.load(std::memory_order_acquire);
    return (write_local_ - read_cached_) >= capacity_;
  }

  inline T &back() noexcept { return buffer_[write_local_ & mask_]; }

  inline void push() noexcept {
    ++write_local_;
    publish_write(false);
  }

  inline bool push(T t) noexcept {
    if (full()) {
      publish_write(true);
      return false;
    }
    back() = t;
    ++write_local_;
    publish_write(false);
    return true;
  }

  inline void flush() noexcept {
    if (unpublished_push_ == 0) {
      return;
    }
    write_pub_.store(write_local_, std::memory_order_release);
    unpublished_push_ = 0;
    need_publish_.store(false, std::memory_order_relaxed);
  }

private:
  static std::size_t round_up_pow2(std::size_t value) noexcept {
    if (value <= 1) {
      return 2;
    }
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    if constexpr (sizeof(std::size_t) >= 8) {
      value |= value >> 32;
    }
    return value + 1;
  }

  inline void publish_write(bool force) noexcept {
    ++unpublished_push_;
    if (!force && unpublished_push_ < publish_batch_ &&
        !need_publish_.load(std::memory_order_relaxed)) {
      return;
    }
    write_pub_.store(write_local_, std::memory_order_release);
    unpublished_push_ = 0;
    need_publish_.store(false, std::memory_order_relaxed);
  }

private:
  const std::size_t capacity_;
  const std::size_t mask_;
  const std::size_t publish_batch_;

  T *buffer_;

  std::size_t write_local_;            // producer only
  mutable std::size_t write_cached_;   // consumer only
  std::atomic<std::size_t> write_pub_; // P->C
  std::size_t unpublished_push_;       // producer only

  mutable std::size_t read_local_;    // consumer only
  mutable std::size_t read_cached_;   // producer only
  std::atomic<std::size_t> read_pub_; // C->P

  mutable std::atomic<bool> need_publish_; // C 请求 P 立刻发布
};
NSE_ZPP