#pragma once

#include <zpp/namespace.h>
#include <atomic>

NSB_ZPP
/**
 * @brief bit 标记器
 * @note 应用场景：高性能，高并发计算
 *  用于获取线程池中空闲线程，每个线程对应一个bit位。
 * @see "../../tests/STL/benchmark_atomic_hit.cpp"
 *  测试结果3线程生产3线程消费，总吞吐量 7,890,272 严重不匹配 fifo SISO 的 300,000,000 吞吐量级别
 *  考虑不使用atomic保证的非严格的同步，使用非atomic + 公平队列方式实现高并发
 * @note 静态函数，适合非atomic操作
 */
/**
 * @biref 非原子性的位操作
 */
template<typename T>
class bits{
public:
    bits():_bits(0){}
    ~bits() = default;
    /**
     * @brief 非原子操作返回最低位1位置,并清除
     */
    bool pop_lsb(size_t& index){
        if (!_bits)
            return false;
        index = find_lsb(_bits);
        _bits &= ~(static_cast<T>(1) << index);
        return true;
    }
    void clear_bit(T state){
        _bits &= ~state;
    }
    void set_bit(T state){
        _bits |= state;
    }
    bool test_bit(T state){
        return (_bits & state) != 0;
    }
    /**
     * @brief 产生一个第index lsb位 bit 为 1 的数
     */
    bool make_lsb_bit(size_t index, T& t){
        if(index >= sizeof(T) * 8){
            return false;
        }
        t = static_cast<T>(1) << index;
        return true;
    }
private:

    /**
     * @brief 查找最高位的1的位置
     * @param num 要查找的数字
     * @return 最高位1的索引，如果num为0则返回-1
     */
    size_t find_msb(T num) {
        if (num == 0) return -1;
    #if defined(__GNUC__) || defined(__clang__)
        // GCC 或 Clang 编译器
        // if constexpr是C++17引入的特性，它在编译时进行条件判断，在运行时没有任何条件判断开销
        // 编译器会根据条件只生成满足条件的代码分支，不满足条件的分支根本不会被编译
        if constexpr (sizeof(T) <= sizeof(unsigned int)) {
            return 31 - __builtin_clz(static_cast<unsigned int>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            return 63 - __builtin_clzl(static_cast<unsigned long>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
            return 63 - __builtin_clzll(static_cast<unsigned long long>(num));
        }
    #elif defined(_MSC_VER)
        // MSVC 编译器
        unsigned long result;
        if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            _BitScanReverse(&result, static_cast<unsigned long>(num));
            return static_cast<int>(result);
        } else if constexpr (sizeof(T) <= sizeof(unsigned __int64)) {
            #if defined(_WIN64)
            _BitScanReverse64(&result, static_cast<unsigned __int64>(num));
            return static_cast<int>(result);
            #else
            // 在32位MSVC上模拟64位BitScanReverse
            unsigned long high = static_cast<unsigned long>(num >> 32);
            if (high != 0) {
                _BitScanReverse(&result, high);
                return static_cast<int>(result) + 32;
            } else {
                unsigned long low = static_cast<unsigned long>(num);
                _BitScanReverse(&result, low);
                return static_cast<int>(result);
            }
            #endif
        }
    #else
        int msb = -1;
        while (num) {
            num >>= 1;
            msb++;
        }
        return msb;
    #endif
    }

    /**
     * @brief 查找最低位的1的位置
     * @param num 要查找的数字
     * @return 最低位1的索引，如果num为0则返回-1
     */
    size_t find_lsb(T num) {
        if (num == 0) return -1;    
    #if defined(__GNUC__) || defined(__clang__)
        // 查找最低位的1（LSB - Least Significant Bit）
        //  ctz: "Count Trailing Zeros"（计数尾随零）
        if constexpr (sizeof(T) <= sizeof(unsigned int)) {
            return __builtin_ctz(static_cast<unsigned int>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            return __builtin_ctzl(static_cast<unsigned long>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
            return __builtin_ctzll(static_cast<unsigned long long>(num));
        }
    #elif defined(_MSC_VER)
        // MSVC 编译器
        unsigned long result;
        if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            _BitScanForward(&result, static_cast<unsigned long>(num));
            return static_cast<int>(result);
        } else if constexpr (sizeof(T) <= sizeof(unsigned __int64)) {
            #if defined(_WIN64)
            _BitScanForward64(&result, static_cast<unsigned __int64>(num));
            return static_cast<int>(result);
            #else
            // 在32位MSVC上模拟64位BitScanForward
            unsigned long low = static_cast<unsigned long>(num);
            if (low != 0) {
                _BitScanForward(&result, low);
                return static_cast<int>(result);
            } else {
                unsigned long high = static_cast<unsigned long>(num >> 32);
                _BitScanForward(&result, high);
                return static_cast<int>(result) + 32;
            }
            #endif
        }
    #else
        // 通用实现 - 查找最低位的1
        size_t index = 0;
        T mask = 1;
        while ((num & mask) == 0) {
            mask <<= 1;
            index++;
        }
        return index;
    #endif
    }
private:
    T _bits;
};

template<typename T>
class atomic_bit{
public:
    atomic_bit(){
        _bits.store(0);
    }
    ~atomic_bit() = default;
        
    /**
     * @brief 检查`state`标记位置是否被设置,`state`必须有且只能有一位是 1
     * @retval false 未被设置 1
     * @retval true 标记位为 1
     */
    bool test_bit(T state) noexcept {
        return (_bits.load(std::memory_order_relaxed) & state) != 0;
    }
    void clear_bit(T state) noexcept{
        _bits.fetch_and(~state, std::memory_order_release);
    }
    /**
     * @brief 设置对应的bit位
     * @note 应用场景：高性能，高并发计算
     *  每个线程都有自己的 bit 标记，对一唯一state。
     */
    void set_bit(T state) noexcept{
#if 1
        _bits.fetch_or(state, std::memory_order_release);
#else
        T old_state = _bits.load(std::memory_order_relaxed);
        T new_state;
        do {
            new_state = old_state | state;
        } while (!_bits.compare_exchange_weak(old_state, new_state, 
            std::memory_order_acquire,
            std::memory_order_relaxed));
#endif
      }

      /**
       * @brief 获取 bit == 1 的位置
       * @note 应用场景：高性能，高并发计算
       *  任何线程试图原子操作获取，当前空闲线程的index并读取后设置0防止被其他线程也读取；
       */
    bool pop_msb(size_t &index) noexcept{
        T old_state = _bits.load(std::memory_order_relaxed);
        T new_state;
        
        do {
            if (old_state == 0) {
                // 如果状态被其他线程变为0，退出循环并返回false
                return false;
            }
            index = find_msb(old_state);
            new_state = old_state & ~(static_cast<T>(1) << index);
            
        } while (!_bits.compare_exchange_weak(old_state, new_state, 
                                               std::memory_order_acquire,
                                               std::memory_order_relaxed));
        
        return true;
    }

    /**
     * @brief 只尝试一次，如果失败则采用公平队列
     * @note 这种方案可能实际效果会更好
     */
    bool try_pop_lsb(size_t &index) noexcept {
        T bits = _bits.load(std::memory_order_relaxed);
        if (bits == 0) {
            return false;
        }
        index = find_lsb(bits);
        return _bits.compare_exchange_strong(bits, bits & ~(static_cast<T>(1) << index), 
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed);
    }
     /**
     * @brief 获取最低位的1的位置并清除该位
     * @param states 原子状态
     * @param index 输出参数，存储找到的最低位1的索引
     * @return 如果找到bit为1的位置返回true，否则返回false
     */
    bool pop_lsb(size_t &index){
        T bits = _bits.load(std::memory_order_relaxed);
    
        while (bits != 0) {
            index = find_lsb(bits);
            if (_bits.compare_exchange_weak(bits, bits & ~(static_cast<T>(1) << index), 
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
                return true;
            }
        }
        
        return false;
    }
    /**
     * @brief 产生一个第index lsb位 bit 为 1 的数
     */
    bool make_lsb_bit(size_t index, T& t){
        if(index >= sizeof(T) * 8){
            return false;
        }
        t = static_cast<T>(1) << index;
        return true;
    }

    void wait_for_any_bit(T mask){
        T old = _bits.load(std::memory_order_relaxed);
        while ((old & mask) == 0) {
            _bits.wait(old, std::memory_order_relaxed);
            old = _bits.load(std::memory_order_relaxed);
        }
    }

    void notify_all(){
        _bits.notify_all();
    }

private:

    /**
     * @brief 查找最高位的1的位置
     * @param num 要查找的数字
     * @return 最高位1的索引，如果num为0则返回-1
     */
    size_t find_msb(T num) {
        if (num == 0) return -1;
    #if defined(__GNUC__) || defined(__clang__)
        // GCC 或 Clang 编译器
        // if constexpr是C++17引入的特性，它在编译时进行条件判断，在运行时没有任何条件判断开销
        // 编译器会根据条件只生成满足条件的代码分支，不满足条件的分支根本不会被编译
        if constexpr (sizeof(T) <= sizeof(unsigned int)) {
            return 31 - __builtin_clz(static_cast<unsigned int>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            return 63 - __builtin_clzl(static_cast<unsigned long>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
            return 63 - __builtin_clzll(static_cast<unsigned long long>(num));
        }
    #elif defined(_MSC_VER)
        // MSVC 编译器
        unsigned long result;
        if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            _BitScanReverse(&result, static_cast<unsigned long>(num));
            return static_cast<int>(result);
        } else if constexpr (sizeof(T) <= sizeof(unsigned __int64)) {
            #if defined(_WIN64)
            _BitScanReverse64(&result, static_cast<unsigned __int64>(num));
            return static_cast<int>(result);
            #else
            // 在32位MSVC上模拟64位BitScanReverse
            unsigned long high = static_cast<unsigned long>(num >> 32);
            if (high != 0) {
                _BitScanReverse(&result, high);
                return static_cast<int>(result) + 32;
            } else {
                unsigned long low = static_cast<unsigned long>(num);
                _BitScanReverse(&result, low);
                return static_cast<int>(result);
            }
            #endif
        }
    #else
        int msb = -1;
        while (num) {
            num >>= 1;
            msb++;
        }
        return msb;
    #endif
    }

    /**
     * @brief 查找最低位的1的位置
     * @param num 要查找的数字
     * @return 最低位1的索引，如果num为0则返回-1
     */
    size_t find_lsb(T num) {
        if (num == 0) return -1;    
    #if defined(__GNUC__) || defined(__clang__)
        // 查找最低位的1（LSB - Least Significant Bit）
        //  ctz: "Count Trailing Zeros"（计数尾随零）
        if constexpr (sizeof(T) <= sizeof(unsigned int)) {
            return __builtin_ctz(static_cast<unsigned int>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            return __builtin_ctzl(static_cast<unsigned long>(num));
        } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
            return __builtin_ctzll(static_cast<unsigned long long>(num));
        }
    #elif defined(_MSC_VER)
        // MSVC 编译器
        unsigned long result;
        if constexpr (sizeof(T) <= sizeof(unsigned long)) {
            _BitScanForward(&result, static_cast<unsigned long>(num));
            return static_cast<int>(result);
        } else if constexpr (sizeof(T) <= sizeof(unsigned __int64)) {
            #if defined(_WIN64)
            _BitScanForward64(&result, static_cast<unsigned __int64>(num));
            return static_cast<int>(result);
            #else
            // 在32位MSVC上模拟64位BitScanForward
            unsigned long low = static_cast<unsigned long>(num);
            if (low != 0) {
                _BitScanForward(&result, low);
                return static_cast<int>(result);
            } else {
                unsigned long high = static_cast<unsigned long>(num >> 32);
                _BitScanForward(&result, high);
                return static_cast<int>(result) + 32;
            }
            #endif
        }
    #else
        // 通用实现 - 查找最低位的1
        size_t index = 0;
        T mask = 1;
        while ((num & mask) == 0) {
            mask <<= 1;
            index++;
        }
        return index;
    #endif
    }

    // 确保 _bits 不会与其他数据共享同一缓存行。
    //alignas(std::hardware_destructive_interference_size) std::atomic<T> _bits;
    alignas(64) std::atomic<T> _bits;
};


NSE_ZPP