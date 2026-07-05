#pragma once

#include <cstddef>
#include <folly/io/IOBuf.h>
#include <memory>
#include <mutex>
#include <vector>
#include <zpp/namespace.h>

NSB_FOLLY

class io_buf {
public:
  io_buf(size_t buf_size, size_t capacity = 256) : buf_size_(buf_size) {
    free_.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
      auto p = static_cast<uint8_t *>(operator new[](buf_size));
      free_.push_back(p);
    }
  }

  ~io_buf() {
    for (void *p : free_) {
      operator delete[](p);
    }
  }

  // Acquire raw block (caller gets ownership until release())
  uint8_t *acquire() {
    std::lock_guard lk(mu_);
    if (!free_.empty()) {
      auto p = static_cast<uint8_t *>(free_.back());
      free_.pop_back();
      return p;
    }
    return static_cast<uint8_t *>(operator new[](buf_size_));
  }

  void release(void *p) noexcept {
    if (!p)
      return;
    std::lock_guard lk(mu_);
    free_.push_back(p);
  }

  // Create a folly::IOBuf that will return memory to this pool when freed.
  std::unique_ptr<folly::IOBuf> takeIOBuf() {
    auto mem = acquire();
    // folly::IOBuf::takeOwnership expects a function pointer with signature
    // void (*freeFunc)(void*, void*) and a userData pointer.
    return folly::IOBuf::takeOwnership(mem, buf_size_, &io_buf::iobufFreeFunc,
                                       this);
  }

private:
  static void iobufFreeFunc(void *mem, void *userData) noexcept {
    auto pool = static_cast<io_buf *>(userData);
    pool->release(mem);
  }

  size_t buf_size_{0};
  std::vector<void *> free_;
  std::mutex mu_;
};

NSE_FOLLY