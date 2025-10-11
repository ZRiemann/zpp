#pragma once
/**
 * @defgroup zpp
 * @defgroup stl
 * @ingroup zpp
 * @defgroup fifo
 * @ingroup stl
 * @{
 * @brief A general fifo implement, 0 memcopy
 * @see benchmark
 * SISO 373M Q/S
 * 19 10:49:38.952952 1421235 I consum 161212671197184 use 431,734,057,965 us. 373,000,000 q/s	stl_svr.cpp:68
 * 20 10:53:53.282619 1421235 I consum 193381976244224 use 518,388,387,632 us. 373,000,000 q/s	stl_svr.cpp:68
 * @attention
 *   fifo is very fast and lock-free for 1I/1O.
 *   though it's not multiI/multiO thread safe and needs use carefully,
 *  效率优先，0拷贝，I/O无锁，代码精炼。
 * @note
 *  为什么要拆分 front()/back() 和 push()/pop() 接口？ 0拷贝，大数据块避免内存复制。
 *  为什么设置 push(T& t)/pop(T& t)? 小数据适合非0拷贝接口。
 *  为什么不设计成dequeue？ 应为效率优先，dequeue会带来队列维护的代码复杂性造成性能损失。
 *  如何在多线程中使用mimo？ 使用2个spin_lock，分别控制push/pop操作。由于代码及其简洁，mimo效率也极高。
 *  为什么一定要线判断是否为空 fifo::empty() 带来编程复杂性？还是效率优先，避免多运行不必要的指令。
 * @note
 *  设置 chunk_capacity = 2, 实现一个环形缓冲区。本质就是一个大环形缓冲区，无限扩大对于实际项目没有意义。
 * @see tests/FIFO/stl_svr.cpp::test_fifo;
 *  test chunk alloc/free pass
 * @attention 
 * T* _back_ptr; not_empty()存在一定操作访问写指针地址的非原子性bug,被_atm_back_ptr取代.
 * 19 14:53:36.845181 2285029 I consumer que:0x56153adaa2f0, 0x56153adaa2f0        thread_pool.h:42
 * 19 14:53:36.945280 2285030 I producer que:0x56153adaa2f0 0x56153adaa2f0 thrp_svr.cpp:68
 * 19 14:53:36.945511 2285030 I thr[0] push [65536] failed thrp_svr.cpp:72
 * 19 14:53:36.945512 2285030 I front_chk:0x56153adbce00->0x56153adcce40 back_chk:0x7173e8050a40->0x0      thrp_svr.cpp:73
 * 19 14:53:36.945513 2285030 I front_ptr:0x56153adbce08 back_ptr:0x7173e8060a48   thrp_svr.cpp:77
 * 19 14:53:36.945513 2285030 I front_end_ptr:0x56153adcce08 back_end_ptr:0x7173e8060a48   thrp_svr.cpp:79
 * 19 14:53:36.945513 2285030 I chunk_num:8 chunk_capacity:8 apare:0x0     thrp_svr.cpp:80
 * 19 14:53:36.945514 2285030 I push_count:65536 pop_count:0       thrp_svr.cpp:81
 * 19 14:53:37.046163 2285030 I thr[0] push [122880] failed        thrp_svr.cpp:72
 * 19 14:53:37.046166 2285030 I front_chk:0x7173e8050a40->0x7173e8040a00 back_chk:0x7173e8080b00->0x0      thrp_svr.cpp:73
 * 19 14:53:37.046167 2285030 I front_ptr:0x7173e8050a48 back_ptr:0x7173e8090b08   thrp_svr.cpp:77
 * 19 14:53:37.046167 2285030 I front_end_ptr:0x7173e8060a48 back_end_ptr:0x7173e8090b08   thrp_svr.cpp:79
 * 19 14:53:37.046167 2285030 I chunk_num:8 chunk_capacity:8 apare:0x0     thrp_svr.cpp:80
 * 19 14:53:37.046168 2285030 I push_count:122880 pop_count:57351  thrp_svr.cpp:81
 * 19 14:53:37.146187 2285030 I thr[0] push [122880] failed        thrp_svr.cpp:72
 * 19 14:53:37.146191 2285030 I front_chk:0x7173e8050a40->0x7173e8040a00 back_chk:0x7173e8080b00->0x0      thrp_svr.cpp:73
 * 19 14:53:37.146192 2285030 I front_ptr:0x7173e8050a48 back_ptr:0x7173e8090b08   thrp_svr.cpp:77
 * 19 14:53:37.146192 2285030 I front_end_ptr:0x7173e8060a48 back_end_ptr:0x7173e8090b08   thrp_svr.cpp:79
 * 19 14:53:37.146192 2285030 I chunk_num:8 chunk_capacity:8 apare:0x0     thrp_svr.cpp:80
 * 19 14:53:37.146193 2285030 I push_count:122880 pop_count:57351  thrp_svr.cpp:81
 * 
 * std::atomic<T*> _atm_back_ptr; // Fix: push满,无法pop问题!
 * 19 14:57:57.344672 2285628 I consumer que:0x6473e7e982f0, 0x6473e7e982f0        thread_pool.h:42
 * 19 14:57:57.444815 2285629 I producer que:0x6473e7e982f0 0x6473e7e982f0 thrp_svr.cpp:68
 * 19 14:57:57.468246 2285628 I pop [262144]       thread_pool.h:89
 * 19 14:57:57.495925 2285628 I pop [524288]       thread_pool.h:89
 * 19 14:57:57.522385 2285628 I pop [786432]       thread_pool.h:89
 * 19 14:57:57.544117 2285629 I p_thr[0] push 1000000 tasks done.  thrp_svr.cpp:90
 */
#include <zpp/namespace.h>
#include <zpp/types.h>
#include <zpp/error.h>
#include <zpp/system/spin.h>
#include <zpp/core/memory/aligned_malloc.hpp>
#include <atomic>
#include <cstdlib>

#define ENABLE_FIFO_TRACE 0
#if ENABLE_FIFO_TRACE
#include <zpp/spdlog.h>
#include <zpp/system/tid.h>
#endif

// 修改宏定义部分，默认使用 atomic 版本，但允许通过编译选项切换
#ifndef FIFO_BACK_PTR_MODE
#define FIFO_BACK_PTR_MODE 2  // 1=atomic, 2=volatile, 3=normal
#endif

NSB_ZPP

constexpr size_t FIFO_CHUNK_ALIGMENT = 64;

template<typename T>
class fifo{
public:
    struct alignas(FIFO_CHUNK_ALIGMENT) chunk{
        chunk* next;
        T data[0];
    };
    chunk *_front_chunk;
    chunk *_back_chunk;

    T* _front_ptr;
    T* _front_end_ptr; // 首部块的最后一项地址，用户快速判断是否需要回收旧块
    T* _back_end_ptr; // 尾部块的最后一项地址，用户快速判断是否需要扩充新块

    size_t _chunk_capacity; // 限制最大值
    size_t _chunk_size; // 块大小

    std::atomic<size_t> _chunk_num; // 单前分配的块数
    std::atomic<chunk*> _spare_chunk; // 置换备用块，提高内存复用率
#if FIFO_BACK_PTR_MODE == 1
    std::atomic<T*> _atm_back_ptr; // Fix: push满,无法pop问题!
#elif FIFO_BACK_PTR_MODE == 2
    T* volatile _back_ptr; // volatile 保证编译器不会优化掉读取操作
#else
    // not_empty()存在一定操作访问写指针地址的非原子性bug,被_atm_back_ptr取代.
    // 会发生 8*8192capacity push满了, pop 条件一直无法满足,可能是消费在线程cache了_back_ptr;
    T* _back_ptr;
#endif
    size_t _count_push;
    size_t _count_pop;
public:
    fifo(fifo&& other) = delete;
    fifo(fifo& other) = delete;

    fifo(bool &valid, size_t chunk_size = 8192, size_t chunk_capacity = 1024)
        : _chunk_capacity(chunk_capacity > 2 ? chunk_capacity : 2)
        , _chunk_size(chunk_size)
        , _chunk_num(0)
        , _count_push(0)
        , _count_pop(0){
        // 分配并初始化首块
        chunk *chk = static_cast<chunk*>(aligned_malloc(sizeof(chunk) + sizeof(T) * _chunk_size, FIFO_CHUNK_ALIGMENT));
        if(!chk){
            // memory insufficient
            _front_chunk = _back_chunk = nullptr;
#if FIFO_BACK_PTR_MODE == 1
            _atm_back_ptr.store(nullptr);
#elif FIFO_BACK_PTR_MODE == 2
            _back_ptr = nullptr;
#else
            _back_ptr = nullptr;
#endif
            _front_ptr = _front_end_ptr = _back_end_ptr = nullptr;
            _spare_chunk.exchange(nullptr);
            valid = false;
            return;
        }
        //spd_inf("alloc chunk[{}]", fmt::ptr(chk));
        ++_chunk_num;
        chk->next = nullptr;
        _front_end_ptr = _back_end_ptr = &chk->data[_chunk_size];
        _front_ptr = &chk->data[0];
#if FIFO_BACK_PTR_MODE == 1
        _atm_back_ptr.store(&chk->data[0]);
#elif FIFO_BACK_PTR_MODE == 2
        _back_ptr = &chk->data[0];
#else
        _back_ptr = &chk->data[0];
#endif
        _front_chunk = _back_chunk = chk;
        // 分配备用块
        chk = static_cast<chunk*>(aligned_malloc(sizeof(chunk) + sizeof(T) * _chunk_size, FIFO_CHUNK_ALIGMENT));
        if(!chk){
            valid = false;
            std::free(_front_chunk);
            _spare_chunk.exchange(nullptr);
            _front_chunk = nullptr;
            return;
        }
        _spare_chunk.exchange(chk);
        ++_chunk_num;
        valid = true;
    }

    ~fifo(){
        //spd_inf("being ~fifo()...");
        std::free(_spare_chunk.exchange(nullptr));
        chunk *chk = _front_chunk;
        chunk *chk_next{nullptr};
        while(chk){
            chk_next = chk->next;
            std::free(chk);
            chk = chk_next;
        }
        //spd_inf("~fifo() done.");
    }
    /**
     * @brief 测试是否可弹出
     */
    bool not_empty() const noexcept{
#if FIFO_BACK_PTR_MODE == 1
        T* back_ptr = _atm_back_ptr.load(std::memory_order_relaxed);
        return _front_ptr != back_ptr && back_ptr != _front_end_ptr;
#elif FIFO_BACK_PTR_MODE == 2
        // 在SPSC场景中，使用内存屏障确保可见性
        std::atomic_thread_fence(std::memory_order_acquire);
#if ENABLE_FIFO_TRACE
        bool ret = _front_ptr != _back_ptr && _back_ptr != _front_end_ptr;
        spd_inf("tid[{}] not_empty?[{}] _front_ptr[{}] _back_ptr[{}] _front_end_ptr[{}] _back_end_ptr[{}]", tid::id(), ret, fmt::ptr(_front_ptr), fmt::ptr(_back_ptr), fmt::ptr(_front_end_ptr), fmt::ptr(_back_end_ptr));
        return ret;
#else
        return _front_ptr != _back_ptr && _back_ptr != _front_end_ptr;
#endif
#else
        return _front_ptr != _back_ptr && _back_ptr != _front_end_ptr;
#endif
    }

    inline bool try_extern_chunk() noexcept{
        chunk *chk = _spare_chunk.exchange(nullptr);
        if(chk){
            chk->next = NULL;
            _back_chunk->next = chk;
            _back_chunk = chk;
            _back_end_ptr = &chk->data[_chunk_size];
#if FIFO_BACK_PTR_MODE == 1
            _atm_back_ptr.store(&chk->data[0]);
#elif FIFO_BACK_PTR_MODE == 2
            _back_ptr = &chk->data[0];
            std::atomic_thread_fence(std::memory_order_release);
#else
            _back_ptr = &chk->data[0];
#endif
            return true;
        }else if(_chunk_num < _chunk_capacity){
            chk = static_cast<chunk*>(aligned_malloc(sizeof(chunk) + sizeof(T) * _chunk_size, FIFO_CHUNK_ALIGMENT));
            if(!chk){
                return false;
            }
            ++_chunk_num;
            chk->next = NULL;
            _back_chunk->next = chk;
            _back_chunk = chk;
            _back_end_ptr = &chk->data[_chunk_size];
#if FIFO_BACK_PTR_MODE == 1
            _atm_back_ptr.store(&chk->data[0]);
#elif FIFO_BACK_PTR_MODE == 2
            _back_ptr = &chk->data[0];
            std::atomic_thread_fence(std::memory_order_release);
#else
            _back_ptr = &chk->data[0];
#endif
            return true;
        }
        return false;
    }
    /**
     * @brief 测试未达到 capacity 限制顶峰；
     */
    bool not_full() noexcept {
#if FIFO_BACK_PTR_MODE == 1
        return (_atm_back_ptr.load(std::memory_order_relaxed) != _back_end_ptr) ? true : try_extern_chunk();
#else
        return (_back_ptr != _back_end_ptr) ? true : try_extern_chunk();
#endif
    }

    /**
     * @brief 访问首项
     * @note 首先查询 empty(), 才能确定首地址是否有数据；
     */
    T& front() noexcept{
        return *_front_ptr;
    }
    /**
     * @brief 访问尾项
     */
    T& back() noexcept {
#if FIFO_BACK_PTR_MODE == 1
        return *_atm_back_ptr.load(std::memory_order_relaxed);
#elif FIFO_BACK_PTR_MODE == 2
        return *const_cast<T*>(_back_ptr);
#else
        return *_back_ptr;
#endif
    }
    /**
     * @brief 压入尾部，尾部指针后移
     * @code
     * void test_push(fifo<int> &que){
     *  if(que.not_full()){ // 必须进行测试，效率优先
     *      int &i = que.back();
     *      i = 3;
     *      que.push();
     *  }
     * }
     * @endcode
     */
    void push() noexcept{
        ++_count_push;
#if FIFO_BACK_PTR_MODE == 1
        ++_atm_back_ptr;
#elif FIFO_BACK_PTR_MODE == 2
        _back_ptr = _back_ptr + 1;
        // 使用内存屏障确保其他线程可见
        std::atomic_thread_fence(std::memory_order_release);
#else
        ++_back_ptr;
#endif

#if FIFO_BACK_PTR_MODE == 1
        if(_atm_back_ptr.load(std::memory_order_relaxed) == _back_end_ptr) {try_extern_chunk();}
#else
        if(_back_ptr == _back_end_ptr) {try_extern_chunk();}
#endif
    }

    /**
     * @brief 弹出头部
     * @note 必须先测试empty() 否则会队列溢出尾部。
     * @code
     * void test_pop(fifo<int> &que){
     *  if(que.not_empty()){ // 必须进行测试，效率优先
     *      int i = que.front();
     *      // read i data; 但是不能保存 i指针地址后续使用，可能块会被释放；
     *      que.pop();
     *  }
     * }
     * @endcode
     */
    void pop() noexcept {
        if(++_front_ptr >= _front_end_ptr){
            // 到达块尾部
            if(_front_chunk->next){
                // 有后续块
                //spd_inf("_front_ptr == _front_end_ptr buf goto next chunk");
                chunk *chk = _front_chunk;
                _front_chunk = _front_chunk->next;
                _front_ptr = &_front_chunk->data[0];
                _front_end_ptr = &_front_chunk->data[_chunk_size];
                chunk *spare = _spare_chunk.exchange(chk);
                if(spare){
                    --_chunk_num;
                    std::free(spare);
                }
                //spd_inf("thr[{}] pop: _front_ptr[{}] >= _front_end_ptr[{}] have next!", tid::id(), fmt::ptr(_front_ptr), fmt::ptr(_front_end_ptr));
                ++_count_pop;
            }else{
                // 运行时bug，可能情况
                // 1. 检测使用是否规范，必须先测试not_full/empty(){在成功内部操作；}； 
                // 2. fifo 代码有BUG。 通过严格的测试来保证 fifo 库的可靠性。
                //spd_inf("thr[{}] pop: _front_ptr[{}] >= _front_end_ptr[{}], No next need fix GUB!", tid::id(), fmt::ptr(_front_ptr), fmt::ptr(_front_end_ptr));
            }
        }else{
            //spd_inf("thr[{}] pop: _front_ptr[{}] < _front_end_ptr[{}] normal!", tid::id(), fmt::ptr(_front_ptr), fmt::ptr(_front_end_ptr));
            ++_count_pop;
        }
    }

    std::string& info(std::string& str) noexcept{
        str += fmt::format("fifo info: push[{}] pop[{}] size[{}]\n\tfront_chunk[{}] back_chunk[{}]\n\tfront_ptr[{}] front_end_ptr[{}]\n\tback_ptr[{}] back_end_ptr[{}]", 
            _count_push, _count_pop, _count_push - _count_pop, fmt::ptr(_front_chunk), fmt::ptr(_back_chunk), 
            fmt::ptr(_front_ptr), fmt::ptr(_front_end_ptr), fmt::ptr(_back_ptr), fmt::ptr(_back_end_ptr));
        return str;
    }
public: // 非0拷贝(有内存拷贝），小内存数据简易接口(参考小于16或32字节，如基本数据类型，指针，小空间占用的数据结构)
    bool push(T t) noexcept {
        if(not_full()){
            back() = t;
            push();
            return true;
        }
        return false;
    }
    bool pop(T &t) noexcept{
        if(not_empty()){
            t = front();
            pop();
            //spd_inf("thr[{}] pop OK!", tid::id());
            return true;
        }
        return false;
    }

    chunk* pop_chunk(T*& first_ptr, T*& last_ptr) noexcept {
        if(!not_empty() || _front_chunk == _back_chunk){
            return nullptr;
        }
        chunk *chk = _front_chunk;
        last_ptr = _front_end_ptr;
        first_ptr = _front_ptr;
        _front_chunk = _front_chunk->next;
        _front_ptr = &_front_chunk->data[0];
        _front_end_ptr = &_front_chunk->data[_chunk_size];
        _count_pop += static_cast<size_t>(last_ptr - first_ptr);
        return chk;
    }
    void free_chunk(chunk* chk) noexcept {
        if(chk){
            chunk *spare = _spare_chunk.exchange(chk);
            if(spare){
                --_chunk_num;
                std::free(spare);
            }
        }
    }
};

/**
 * @brief 单生产者单消费者模式
 */
template<typename T>
using spsc = fifo<T>;

/**
 * @brief 单生产者多消费者队列
 * @note 适合自定义线程池
 */
template<typename T>
class spmc{
public:
    fifo<T> _fifo;
    spin _spin_p;
public:
    /**
     * @brief 创建多生产者单消费者队列包装器
     * @param fifo 被包装的队列引用，其生命周期必须长于此对象
     */
    spmc(bool &valid, size_t chunk_size = 8192, size_t chunk_capacity = 1024):_fifo(valid, chunk_size, chunk_capacity){}
    ~spmc() = default;

    inline bool not_full() noexcept {
        return _fifo.not_full();
    }
    inline bool not_empty() noexcept {
        return _fifo.not_empty();
    }
    inline bool push(T t) noexcept {
        return _fifo.push(t);
    }
    inline bool pop(T &t) noexcept {
        std::unique_lock<spin> lock(_spin_p);
        return _fifo.pop(t);
    }
    inline std::string& info(std::string& str) noexcept {
        return _fifo.info(str);
    }

    inline fifo<T>::chunk* pop_chunk(T*& first_ptr, T*& last_ptr) noexcept {
        std::unique_lock<spin> lock(_spin_p);
        return _fifo.pop_chunk(first_ptr, last_ptr);
    }
    inline void free_chunk(fifo<T>::chunk* chk) noexcept {
        _fifo.free_chunk(chk);
    }
};

/**
 * @brief 多生产者单消费者队列
 * @note 适合自定义线程池
 */
template<typename T>
class mpsc{
public:
    bool _valid{false};
    fifo<T> _fifo;
    spin _spin_p;
public:
    /**
     * @brief 创建多生产者单消费者队列包装器
     * @param fifo 被包装的队列引用，其生命周期必须长于此对象
     */
    mpsc(size_t chunk_size = 8192, size_t chunk_capacity = 1024):_fifo(_valid, chunk_size, chunk_capacity){}
    mpsc(bool &valid, size_t chunk_size = 8192, size_t chunk_capacity = 1024):_fifo(valid, chunk_size, chunk_capacity){}
    ~mpsc() = default;

    inline bool not_full() noexcept {
        return _fifo.not_full();
    }
    inline bool not_empty() noexcept {
        return _fifo.not_empty();
    }
    inline bool push(T t) noexcept {
        std::unique_lock<spin> lock(_spin_p);
        return _fifo.push(t);
    }
    inline bool pop(T &t) noexcept {
        return _fifo.pop(t);
    }
    inline std::string& info(std::string& str) noexcept {
        return _fifo.info(str);
    }

    inline fifo<T>::chunk* pop_chunk(T*& first_ptr, T*& last_ptr) noexcept {
        return _fifo.pop_chunk(first_ptr, last_ptr);
    }
    inline void free_chunk(fifo<T>::chunk* chk) noexcept {
        _fifo.free_chunk(chk);
    }

};
/**
 * @brief 多生产者多消费者队列
 * @note 适合对象池
 */
template<typename T>
class mpmc{
public:
    bool _valid{false};
    fifo<T> _fifo;
    spin _spin_c;
    spin _spin_p;
public:
    mpmc(size_t chunk_size = 8192, size_t chunk_capacity = 1024):_fifo(_valid, chunk_size, chunk_capacity){}
    mpmc(bool &valid, size_t chunk_size = 8192, size_t chunk_capacity = 1024):_fifo(valid, chunk_size, chunk_capacity){}
    ~mpmc() = default;

    inline bool not_full() noexcept {
        return _fifo.not_full();
    }
    inline bool not_empty() noexcept {
        return _fifo.not_empty();
    }
    inline bool push(T t) noexcept {
        std::unique_lock<spin> lock(_spin_p);
        return _fifo.push(t);
    }
    inline bool pop(T &t) noexcept {
        std::unique_lock<spin> lock(_spin_c);
        return _fifo.pop(t);
    }
    inline std::string& info(std::string& str) noexcept {
        return _fifo.info(str);
    }

    inline fifo<T>::chunk* pop_chunk(T*& first_ptr, T*& last_ptr) noexcept {
        std::unique_lock<spin> lock(_spin_c);
        return _fifo.pop_chunk(first_ptr, last_ptr);
    }

    inline void free_chunk(fifo<T>::chunk* chk) noexcept {
        _fifo.free_chunk(chk);
    }

};
/**
 * @}
 */

NSE_ZPP
