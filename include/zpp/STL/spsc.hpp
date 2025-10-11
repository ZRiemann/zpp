#pragma once

#include <zpp/namespace.h>
#include <zpp/types.h>
#include <zpp/error.h>
#include <zpp/system/spin.h>
#include <zpp/core/memory/aligned_malloc.hpp>
#include <atomic>
#include <cstdlib>

NSB_ZPP

constexpr std::size_t SPSC_CHUNK_ALIGMENT = 64;

template<typename T>
class spsc{
public:
    struct alignas(SPSC_CHUNK_ALIGMENT) chunk{
        chunk* next;
        T data[0];
    };
public: // constructors & destructors
    spsc(spsc&& other) = delete;
    spsc(spsc& other) = delete;

    /**
     * @brief 动态内存分配，适合读写速度匹配；
     */
    spsc(std::size_t chunk_size, std::size_t chunk_capacity)
        : _chunk_size(chunk_size > 0 ? chunk_size : 1)
        , _chunk_capacity(chunk_capacity > 2 ? chunk_capacity : 2)
        , _chunk_num(0)
        , _spare_chunk(nullptr)
        , _front_chunk(nullptr)
        , _back_chunk(nullptr)
        , _front_ptr(nullptr)
        , _front_end_ptr(nullptr)
        , _back_local(nullptr)
        , _back_ptr(nullptr)
        , _back_cached(nullptr)
        , _back_end_ptr(nullptr){

        chunk* chk = alloc_chunk();
        if(chk){
            _front_chunk = _back_chunk = chk;
            _front_ptr = &_front_chunk->data[0];
            _back_local = &_back_chunk->data[0];
            _back_ptr.store(_back_local, std::memory_order_relaxed);
            _back_cached = _back_local;
            _front_end_ptr = _back_end_ptr = &_back_chunk->data[_chunk_size];       
        }
    }
    /**
     * @brief 静态内存分配，避免内存分配开销，提高读写效率；
     * @note 读速度不小与写速度的一半，否则可能出现读写不匹配导致的性能问题；
     */
    spsc(std::size_t capacity)
        :spsc(capacity / 2, 2){}
    ~spsc(){
        aligned_free(_spare_chunk.exchange(nullptr));
        chunk *chk = _front_chunk;
        chunk *chk_next{nullptr};
        while(chk){
            chk_next = chk->next;
            aligned_free(chk);
            chk = chk_next;
        }
    }

    std::size_t approx_size() const noexcept {
        std::size_t size = 0;
        size = _chunk_num.load(std::memory_order_relaxed) * _chunk_size;
        if(_spare_chunk.load(std::memory_order_relaxed)){
            size -= _chunk_size;
        }
        size -= _front_ptr - &_front_chunk->data[0];
        T* back_ptr = _back_ptr.load(std::memory_order_acquire);
        size -= _back_end_ptr - back_ptr;
        return size;
    }
public: // Consume
    inline bool empty() const noexcept {
        if(_front_ptr != _back_cached){
            return false;
        }
        _back_cached = _back_ptr.load(std::memory_order_acquire);
        return _front_ptr == _back_cached;
    }
    inline T& front() noexcept {
        return *_front_ptr;
    }
    inline bool pop() noexcept {
        if(++_front_ptr >= _front_end_ptr){
            if(_front_chunk->next){
                chunk* chk = _front_chunk;
                _front_chunk = _front_chunk->next;
                _front_ptr = &_front_chunk->data[0];
                _front_end_ptr = &_front_chunk->data[_chunk_size];
                free_chunk(chk);
            }else{
                return false; // 异常状态
            }
        }
        return true;
    }

    inline bool pop(T& t) noexcept {
        if(empty()){
            return false;
        }
        t = *_front_ptr;
        return pop();
    }
public: // Produce
    inline bool full() const noexcept {
        return _back_local == _back_end_ptr;
    }

    inline T& back() noexcept {
        return *const_cast<T*>(_back_local);
    }

    inline void push() noexcept {
        _back_local = _back_local + 1;
        if(_back_local >= _back_end_ptr){
            chunk *chk = alloc_chunk();
            if(chk){
                append_chunk(chk);
            }else{
                _back_local = _back_end_ptr; // 标记为满状态
            }
        }
        _back_ptr.store(_back_local, std::memory_order_release);
    }

    inline bool push(T t) noexcept {
        if(full()){
            return false;
        }
        *const_cast<T*>(_back_local) = t;
        push();
        return true;
    }
private: // help functions
    inline chunk* alloc_chunk() noexcept {
        chunk *chk = _spare_chunk.exchange(nullptr);
        if(chk){
            return chk;
        }
        if(_chunk_num >= _chunk_capacity){
            return nullptr;
        }
        chk = static_cast<chunk*>(aligned_malloc(sizeof(chunk) + sizeof(T) * _chunk_size, SPSC_CHUNK_ALIGMENT));
        if(chk){
            ++_chunk_num;
        }
        return chk;
    }

    inline void free_chunk(chunk* chk) noexcept {
        chunk* spare = _spare_chunk.exchange(chk);
        if(spare){
            aligned_free(spare);
            --_chunk_num;
        }
    }

    inline void append_chunk(chunk* chk) noexcept {
        chk->next = nullptr;
        _back_chunk->next = chk;
        _back_chunk = chk;
        _back_end_ptr = &chk->data[_chunk_size];
        _back_local = &chk->data[0];
    }
private:
    std::size_t _chunk_size;
    std::size_t _chunk_capacity;

    std::atomic<std::size_t> _chunk_num;
    std::atomic<chunk*> _spare_chunk;

    chunk *_front_chunk;
    chunk *_back_chunk;

    T* _front_ptr;
    T* _front_end_ptr;

    T* _back_local; // 仅生产者线程访问
    std::atomic<T*> _back_ptr; // P->C 发布指针，release/acquire 同步
    mutable T* _back_cached; // 仅消费者线程访问，减少 acquire 读取频率
    T* _back_end_ptr;
};

NSE_ZPP