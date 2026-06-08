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
        : chunk_size_(chunk_size > 0 ? chunk_size : 1)
        , chunk_capacity_(chunk_capacity > 2 ? chunk_capacity : 2)
        , chunk_num_(0)
        , spare_chunk_(nullptr)
        , front_chunk_(nullptr)
        , back_chunk_(nullptr)
        , front_ptr_(nullptr)
        , front_end_ptr_(nullptr)
        , back_local_(nullptr)
        , back_ptr_(nullptr)
        , back_cached_(nullptr)
        , back_end_ptr_(nullptr){

        chunk* chk = alloc_chunk();
        if(chk){
            front_chunk_ = back_chunk_ = chk;
            front_ptr_ = &front_chunk_->data[0];
            back_local_ = &back_chunk_->data[0];
            back_ptr_.store(back_local_, std::memory_order_release);
            back_cached_ = back_local_;
            front_end_ptr_ = back_end_ptr_ = &back_chunk_->data[chunk_size_];
        }
    }
    /**
     * @brief 静态内存分配，避免内存分配开销，提高读写效率；
     * @note 读速度不小与写速度的一半，否则可能出现读写不匹配导致的性能问题；
     */
    mpmc(std::size_t capacity)
        :mpmc(capacity / 2, 2){}
    ~mpmc(){
        aligned_free(spare_chunk_.exchange(nullptr));
        chunk *chk = front_chunk_;
        chunk *chk_next{nullptr};
        while(chk){
            chk_next = chk->next;
            aligned_free(chk);
            chk = chk_next;
        }
    }

    std::size_t approx_size() const noexcept {
        std::size_t size = 0;
        size = chunk_num_.load(std::memory_order_acquire) * chunk_size_;
        if(spare_chunk_.load(std::memory_order_acquire)){
            size -= chunk_size_;
        }
        size -= front_ptr_ - &front_chunk_->data[0];
        size -= back_end_ptr_ - back_ptr_.load(std::memory_order_acquire);
        return size;
    }
public:
    inline bool push(T t) noexcept {
        spin_p_.lock();
        if(_full()){
            spin_p_.unlock();
            return false;
        }
        *back_local_ = t;
        push();
        spin_p_.unlock();
        return true;
    }

    inline bool pop(T& t) noexcept {
        spin_c_.lock();
        if(_empty()){
            spin_c_.unlock();
            return false;
        }
        t = *front_ptr_;
        pop();
        spin_c_.unlock();
        return true;
    }

    inline bool empty() noexcept {
        spin_c_.lock();
        bool ret = _empty();
        spin_c_.unlock();
        return ret;
    }

    inline bool full() noexcept {
        spin_p_.lock();
        bool ret = _full();
        spin_p_.unlock();
        return ret;
    }
private: // Consume
    inline bool _empty() noexcept {
        if(front_ptr_ != back_cached_){
            try_move_next_chunk();
            return false;
        }
        back_cached_ = back_ptr_.load(std::memory_order_acquire);
        if(front_ptr_ == back_cached_){
            return true;
        }
        try_move_next_chunk();
        return false;
    }
    inline T& front() noexcept {
        return *front_ptr_;
    }
    inline void pop() noexcept {
        ++front_ptr_;
    }
private: // Produce
    inline bool _full() noexcept {
        if(back_local_ < back_end_ptr_){
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
        return *back_local_;
    }

    inline void push() noexcept {
        ++back_local_;
        back_ptr_.store(back_local_, std::memory_order_release);
    }

#if 0
    inline void print_status() {
        spd_inf("front_ptr:{}, back_ptr:{}, front_end_ptr:{}, back_end_ptr:{}, back_local:{}, back_cached:{}\n"
            "front_chunk:{}, back_chunk:{}, chunk_num:{}, spare_chunk:{}",
            fmt::ptr(front_ptr_), fmt::ptr(back_ptr_.load(std::memory_order_acquire)), fmt::ptr(front_end_ptr_),
            fmt::ptr(back_end_ptr_), fmt::ptr(back_local_), fmt::ptr(back_cached_),
            fmt::ptr(front_chunk_), fmt::ptr(back_chunk_), CTS(chunk_num_.load(std::memory_order_acquire)), fmt::ptr(spare_chunk_.load(std::memory_order_acquire)));
    }
#endif
public: // todo: push_bulk(), pop_bulk();
private: // help functions
    inline chunk* alloc_chunk() noexcept {
        chunk *chk = spare_chunk_.exchange(nullptr, std::memory_order_acquire);
        if(chk){
            chk->next = nullptr;
            return chk;
        }
        if(chunk_num_.load(std::memory_order_acquire) >= chunk_capacity_){
            return nullptr;
        }
        chk = static_cast<chunk*>(aligned_malloc(sizeof(chunk) + sizeof(T) * chunk_size_, MPMC_CHUNK_ALIGMENT));
        if(chk){
            chk->next = nullptr;
            chunk_num_.fetch_add(1, std::memory_order_release);
        }
        return chk;
    }

    inline void free_chunk(chunk* chk) noexcept {
        chk->next = nullptr;
        chunk* spare = spare_chunk_.exchange(chk, std::memory_order_release);
        if(spare){
            aligned_free(spare);
            chunk_num_.fetch_sub(1, std::memory_order_release);
        }
    }

    inline void append_chunk(chunk* chk) noexcept {
        chk->next = nullptr;
        back_chunk_->next = chk;
        back_chunk_ = chk;
        back_end_ptr_ = &chk->data[chunk_size_];
        back_local_ = &chk->data[0];
    }

    inline void try_move_next_chunk() noexcept {
        if(front_ptr_ == front_end_ptr_){// && front_chunk_->next){
            chunk* chk = front_chunk_;
            front_chunk_ = front_chunk_->next;
            front_ptr_ = &front_chunk_->data[0];
            front_end_ptr_ = &front_chunk_->data[chunk_size_];
            free_chunk(chk);
        }
    }
public:
    std::size_t chunk_size_;
    std::size_t chunk_capacity_;

    std::atomic<std::size_t> chunk_num_;
    std::atomic<chunk*> spare_chunk_;

    chunk *front_chunk_;
    chunk *back_chunk_;

    T* front_ptr_;
    T* front_end_ptr_;
    spin spin_c_; // 消费者锁，保护消费者访问核心变量

    char padding_[64]; // 强制隔离消费者和生产者的核心变量

    T* back_local_; // 仅生产者线程访问
    std::atomic<T*> back_ptr_; // P->C 发布指针，release/acquire 同步
    mutable T* back_cached_; // 仅消费者线程访问，减少 acquire 读取频率
    T* back_end_ptr_;
    spin spin_p_; // 生产者锁，保护生产者访问核心变量
};

NSE_ZPP
