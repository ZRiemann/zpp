#pragma once

#include <zpp/namespace.h>
#include <zpp/types.h>
#include <zpp/error.h>
#include <zpp/system/spin.h>
#include <zpp/core/memory/aligned_malloc.hpp>
#include <atomic>
#include <cstdlib>

NSB_ZPP

template<typename T>
class ring{
public:
	ring(ring&& other) = delete;
	ring(ring& other) = delete;

	explicit ring(std::size_t capacity, std::size_t publish_batch = 1)
		: _capacity(round_up_pow2(capacity > 1 ? capacity : 2))
		, _mask(_capacity - 1)
		, _publish_batch(publish_batch > 0 ? publish_batch : 1)
		, _buffer(static_cast<T*>(aligned_malloc(sizeof(T) * _capacity, 64)))
		, _write_local(0)
		, _write_cached(0)
		, _write_pub(0)
		, _unpublished_push(0)
		, _read_local(0)
		, _read_cached(0)
		, _read_pub(0)
		, _need_publish(false) {
	}

	~ring() {
		aligned_free(_buffer);
	}

	std::size_t capacity() const noexcept {
		return _capacity;
	}

	std::size_t approx_size() const noexcept {
		const std::size_t write = _write_pub.load(std::memory_order_acquire);
		const std::size_t read = _read_pub.load(std::memory_order_acquire);
		return write - read;
	}

public: // Consume
	inline bool empty() const noexcept {
		if(_read_local != _write_cached){
			return false;
		}
		_need_publish.store(true, std::memory_order_relaxed);
		_write_cached = _write_pub.load(std::memory_order_acquire);
		return _read_local == _write_cached;
	}

	inline T& front() noexcept {
		return _buffer[_read_local & _mask];
	}

	inline bool pop() noexcept {
		if(empty()){
			return false;
		}
		++_read_local;
		_read_pub.store(_read_local, std::memory_order_release);
		return true;
	}

	inline bool pop(T& t) noexcept {
		if(empty()){
			return false;
		}
		t = front();
		++_read_local;
		_read_pub.store(_read_local, std::memory_order_release);
		return true;
	}

public: // Produce
	inline bool full() const noexcept {
		if((_write_local - _read_cached) < _capacity){
			return false;
		}
		_read_cached = _read_pub.load(std::memory_order_acquire);
		return (_write_local - _read_cached) >= _capacity;
	}

	inline T& back() noexcept {
		return _buffer[_write_local & _mask];
	}

	inline void push() noexcept {
		++_write_local;
		publish_write(false);
	}

	inline bool push(T t) noexcept {
		if(full()){
			publish_write(true);
			return false;
		}
		back() = t;
		++_write_local;
		publish_write(false);
		return true;
	}

	inline void flush() noexcept {
		if(_unpublished_push == 0){
			return;
		}
		_write_pub.store(_write_local, std::memory_order_release);
		_unpublished_push = 0;
		_need_publish.store(false, std::memory_order_relaxed);
	}

private:
	static std::size_t round_up_pow2(std::size_t value) noexcept {
		if(value <= 1){
			return 2;
		}
		--value;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		if constexpr(sizeof(std::size_t) >= 8){
			value |= value >> 32;
		}
		return value + 1;
	}

	inline void publish_write(bool force) noexcept {
		++_unpublished_push;
		if(!force && _unpublished_push < _publish_batch && !_need_publish.load(std::memory_order_relaxed)){
			return;
		}
		_write_pub.store(_write_local, std::memory_order_release);
		_unpublished_push = 0;
		_need_publish.store(false, std::memory_order_relaxed);
	}

private:
	const std::size_t _capacity;
	const std::size_t _mask;
	const std::size_t _publish_batch;

	T* _buffer;

	std::size_t _write_local; // producer only
	mutable std::size_t _write_cached; // consumer only
	std::atomic<std::size_t> _write_pub; // P->C
	std::size_t _unpublished_push; // producer only

	mutable std::size_t _read_local; // consumer only
	mutable std::size_t _read_cached; // producer only
	std::atomic<std::size_t> _read_pub; // C->P

	mutable std::atomic<bool> _need_publish; // C 请求 P 立刻发布
};
NSE_ZPP