#pragma once

#include <atomic>
#include <mutex>
#include <zpp/namespace.h>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

#if defined(__arm__) || defined(__aarch64__)
#include <arm_acle.h>
#endif

NSB_ZPP
/**
 * @brief spin lock
 * @code
 * void test(){
 *  z::spin spin;
 *  std::lock_guard<z::spin> guard(spin);
 *  // ...
 * }
 * @endcode
 * @code
 * void example_with_unique_lock() {
 *  z::spin spin;
 *  std::unique_lock<z::spin> lock(spin);
 *  // 可以手动解锁
 *  lock.unlock();
 *  // 执行一些不需要锁的操作
 *  // ...
 *  // 重新锁定
 *  lock.lock();
 *  // 更多临界区代码
 *  // ...
 * // 离开作用域时自动解锁
 * }
 * @endcode
 * @code
 * std::unique_lock<z::spin> lock(spin, std::try_to_lock);
 * if (lock.owns_lock()) {// 成功获取锁
 * } else {// 未能获取锁，执行替代逻辑
 * }
 * @endcode
 */
class spin{
    std::atomic_flag _flag = ATOMIC_FLAG_INIT;
public:
    spin() = default;
    spin(const spin&) = delete;
    spin& operator= (const spin&) = delete;
    void lock() {
        while(_flag.test_and_set(std::memory_order_acquire)){
            #if defined(__x86_64__) || defined(__i386__)
                // 指令不会导致线程让出执行周期或降低自旋锁的效率，实际上它是为了优化自旋等待而专门设计的
                // 优化自旋等待：它向处理器发出信号，表明当前代码正在执行自旋等待循环
                // 减少能耗：降低处理器在自旋等待期间的能耗
                // 提高多线程性能：减少内存顺序违例（memory-order violations）的惩罚
                // 防止执行单元过热：避免处理器在紧密循环中过热
                _mm_pause(); // Intel/AMD 处理器上的 PAUSE 指令
                // __asm__ __volatile__("pause" ::: "memory");
            #elif defined(__arm__) || defined(__aarch64__)
                __yield(); // ARM 处理器上的 YIELD 指令
                // __asm__ __volatile__("yield" ::: "memory");
            #else
                // 其他架构可能需要不同的指令
                std::this_thread::yield();
            #endif
        }
    }
    bool try_lock() {
        return !_flag.test_and_set(std::memory_order_acquire);
    }
    void unlock() {
        _flag.clear(std::memory_order_release);
    }
};
NSE_ZPP
