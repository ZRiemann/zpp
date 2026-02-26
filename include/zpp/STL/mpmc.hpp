#pragma once

#include <zpp/namespace.h>
#include <zpp/types.h>
#include <zpp/error.h>
#include <zpp/system/spin.h>
#include <zpp/core/memory/aligned_malloc.hpp>
#include <atomic>
#include <cstdlib>

//#include <zpp/spdlog.h>

NSB_ZPP

constexpr std::size_t MPMC_CHUNK_ALIGMENT = 64;

template<typename T>
class mpmc{
public:
    struct alignas(MPMC_CHUNK_ALIGMENT) chunk{
        chunk* next;
        T data[0];
    };
public: // constructors & destructors
    mpmc(mpmc&& other) = delete;
    mpmc(mpmc& other) = delete;

    /**
     * @brief 动态内存分配，适合读写速度匹配；
     */
    mpmc(std::size_t chunk_size, std::size_t chunk_capacity)
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
            _back_ptr.store(_back_local, std::memory_order_release);
            _back_cached = _back_local;
            _front_end_ptr = _back_end_ptr = &_back_chunk->data[_chunk_size];       
        }
    }
    /**
     * @brief 静态内存分配，避免内存分配开销，提高读写效率；
     * @note 读速度不小与写速度的一半，否则可能出现读写不匹配导致的性能问题；
     */
    mpmc(std::size_t capacity)
        :mpmc(capacity / 2, 2){}
    ~mpmc(){
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
        size = _chunk_num.load(std::memory_order_acquire) * _chunk_size;
        if(_spare_chunk.load(std::memory_order_acquire)){
            size -= _chunk_size;
        }
        size -= _front_ptr - &_front_chunk->data[0];
        size -= _back_end_ptr - _back_ptr.load(std::memory_order_acquire);
        return size;
    }
public:
    inline bool push(T t) noexcept {
        _spin_p.lock();
        if(_full()){
            _spin_p.unlock();
            return false;
        }
        *_back_local = t;
        push();
        _spin_p.unlock();
        return true;
    }

    inline bool pop(T& t) noexcept {
        _spin_c.lock();
        if(_empty()){
            _spin_c.unlock();
            return false;
        }
        t = *_front_ptr;
        pop();
        _spin_c.unlock();
        return true;
    }

    inline bool empty() noexcept {
        _spin_c.lock();
        bool ret = _empty();
        _spin_c.unlock();
        return ret;
    }

    inline bool full() noexcept {
        _spin_p.lock();
        bool ret = _full();
        _spin_p.unlock();
        return ret;
    }
private: // Consume
    inline bool _empty() noexcept {
        if(_front_ptr != _back_cached){
            try_move_next_chunk();
            return false;
        }
        _back_cached = _back_ptr.load(std::memory_order_acquire);
        if(_front_ptr == _back_cached){
            return true;
        }
        try_move_next_chunk();
        return false;
    }
    inline T& front() noexcept {
        return *_front_ptr;
    }
    inline void pop() noexcept {
        ++_front_ptr;
    }
private: // Produce
    inline bool _full() noexcept {
        if(_back_local < _back_end_ptr){
            return false;
        }
        chunk* chk = alloc_chunk();
        if(!chk){
            return true;
        }
        append_chunk(chk);
        return false;
    }

    inline T& back() noexcept {
        return *_back_local;
    }

    inline void push() noexcept {
        ++_back_local;
        _back_ptr.store(_back_local, std::memory_order_release);
    }

#if 0
    inline void print_status() {
        spd_inf("front_ptr:{}, back_ptr:{}, front_end_ptr:{}, back_end_ptr:{}, back_local:{}, back_cached:{}\n"
            "front_chunk:{}, back_chunk:{}, chunk_num:{}, spare_chunk:{}", 
            fmt::ptr(_front_ptr), fmt::ptr(_back_ptr.load(std::memory_order_acquire)), fmt::ptr(_front_end_ptr), 
            fmt::ptr(_back_end_ptr), fmt::ptr(_back_local), fmt::ptr(_back_cached),
            fmt::ptr(_front_chunk), fmt::ptr(_back_chunk), CTS(_chunk_num.load(std::memory_order_acquire)), fmt::ptr(_spare_chunk.load(std::memory_order_acquire)));
    }
#endif
public: // todo: push_bulk(), pop_bulk();
private: // help functions
    inline chunk* alloc_chunk() noexcept {
        chunk *chk = _spare_chunk.exchange(nullptr, std::memory_order_acquire);
        if(chk){
            return chk;
        }
        if(_chunk_num.load(std::memory_order_acquire) >= _chunk_capacity){
            return nullptr;
        }
        chk = static_cast<chunk*>(aligned_malloc(sizeof(chunk) + sizeof(T) * _chunk_size, MPMC_CHUNK_ALIGMENT));
        if(chk){
            _chunk_num.fetch_add(1, std::memory_order_release);
        }
        return chk;
    }

    inline void free_chunk(chunk* chk) noexcept {
        chunk* spare = _spare_chunk.exchange(chk, std::memory_order_release);
        if(spare){
            aligned_free(spare);
            _chunk_num.fetch_sub(1, std::memory_order_release);
        }
    }

    inline void append_chunk(chunk* chk) noexcept {
        chk->next = nullptr;
        _back_chunk->next = chk;
        _back_chunk = chk;
        _back_end_ptr = &chk->data[_chunk_size];
        _back_local = &chk->data[0];
    }

    inline void try_move_next_chunk() noexcept {
        if(_front_ptr == _front_end_ptr){// && _front_chunk->next){
            chunk* chk = _front_chunk;
            _front_chunk = _front_chunk->next;
            _front_ptr = &_front_chunk->data[0];
            _front_end_ptr = &_front_chunk->data[_chunk_size];
            free_chunk(chk);
        }
    }
public:
    std::size_t _chunk_size;
    std::size_t _chunk_capacity;

    std::atomic<std::size_t> _chunk_num;
    std::atomic<chunk*> _spare_chunk;

    chunk *_front_chunk;
    chunk *_back_chunk;

    T* _front_ptr;
    T* _front_end_ptr;
    spin _spin_c; // 消费者锁，保护消费者访问核心变量

    char _padding[64]; // 强制隔离消费者和生产者的核心变量

    T* _back_local; // 仅生产者线程访问
    std::atomic<T*> _back_ptr; // P->C 发布指针，release/acquire 同步
    mutable T* _back_cached; // 仅消费者线程访问，减少 acquire 读取频率
    T* _back_end_ptr;
    spin _spin_p; // 生产者锁，保护生产者访问核心变量
};

NSE_ZPP