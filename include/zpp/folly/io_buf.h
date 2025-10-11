#pragma once

#include <zpp/namespace.h>
#include <folly/io/IOBuf.h>
#include <memory>
#include <vector>
#include <mutex>
#include <cstddef>

NSB_FOLLY

class io_buf {
public:
    io_buf(size_t buf_size, size_t capacity = 256)
        : _buf_size(buf_size) {
        _free.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            auto p = static_cast<uint8_t*>(operator new[](buf_size));
            _free.push_back(p);
        }
    }

    ~io_buf() {
        for (void* p : _free) {
            operator delete[](p);
        }
    }

    // Acquire raw block (caller gets ownership until release())
    uint8_t* acquire() {
        std::lock_guard lk(_mu);
        if (!_free.empty()) {
            auto p = static_cast<uint8_t*>(_free.back());
            _free.pop_back();
            return p;
        }
        return static_cast<uint8_t*>(operator new[](_buf_size));
    }

    void release(void* p) noexcept {
        if (!p) return;
        std::lock_guard lk(_mu);
        _free.push_back(p);
    }

    // Create a folly::IOBuf that will return memory to this pool when freed.
    std::unique_ptr<folly::IOBuf> takeIOBuf() {
        auto mem = acquire();
        // folly::IOBuf::takeOwnership expects a function pointer with signature
        // void (*freeFunc)(void*, void*) and a userData pointer.
        return folly::IOBuf::takeOwnership(
            mem,
            _buf_size,
            &io_buf::iobufFreeFunc,
            this);
    }

private:
    static void iobufFreeFunc(void* mem, void* userData) noexcept {
        auto pool = static_cast<io_buf*>(userData);
        pool->release(mem);
    }

    size_t _buf_size{0};
    std::vector<void*> _free;
    std::mutex _mu;
};

NSE_FOLLY