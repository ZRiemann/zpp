#pragma once

#include <zpp/namespace.h>
#include <zpp/system/atomic_bit.h>

NSB_ZPP
class benchmark_atomic_bit {
public:
    benchmark_atomic_bit() = default;
    ~benchmark_atomic_bit() = default;
    void base();
    /**
     * @brief 测试多线程情况下设置与检测效率 Q/S
     */
    void benchmark_mt(size_t thr_num);
    void benchmark_mt_noatomic(size_t thr_num);
private:
    atomic_bit<uint64_t> _bits;
    bits<uint64_t> _bits_natm;
};
NSE_ZPP