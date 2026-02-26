#pragma once

#ifndef USE_BOOST
#include <thread>
using namespace std;
#else
#include <boost/thread/v2/thread.hpp>
using namespace boost;
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
#include <arm_acle.h>
#endif

#include <zpp/namespace.h>

NSB_ZPP

inline void sleep_ms(int ms){
    this_thread::sleep_for(chrono::milliseconds(ms));
}

inline void sleep_us(time_t us){
    this_thread::sleep_for(chrono::microseconds(us));
}

/**
 * @brief 轻量级的睡眠函数，适合短暂等待，避免线程切换开销
 */
inline void pause(int count){
    for(int i = 0; i < count; ++i) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
        __asm__ __volatile__("yield");
#else
        std::this_thread::yield();
#endif
    }
}
NSE_ZPP
