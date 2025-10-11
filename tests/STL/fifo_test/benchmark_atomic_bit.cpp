#include "benchmark_atomic_bit.h"
#include <zpp/spdlog.h>
#include <thread>
#include <zpp/system/sleep.h>

USE_ZPP

void benchmark_atomic_bit::base(){
    _bits.set_bit(static_cast<uint64_t>(0x1));
    _bits.set_bit(static_cast<uint64_t>(0x10));

    size_t index{0};
    if(_bits.pop_lsb(index)){
        spd_inf("pop index[{}]", index);
    }
    if(_bits.pop_lsb(index)){
        spd_inf("pop index[{}]", index);
    }
    if(!_bits.pop_lsb(index)){
        spd_inf("bits is empty now");
    }
}

/**
 * @brief 多线程情况下标记和读取空闲基准测试
 * @note
 * 14 18:10:35.895140 1501566 I thr_c[1] produce done. busy[2677120] idel[147246900]       benchmark_atomic_bit.cpp:70
 * 14 18:10:35.895140 1501568 I thr_c[2] produce done. busy[2064890] idel[200525970]       benchmark_atomic_bit.cpp:70
 * 14 18:10:35.895140 1501565 I thr_p[1] produce done. count_ok[1889874] count_fail[20773911]      benchmark_atomic_bit.cpp:46
 * 14 18:10:35.895140 1501563 I thr_p[0] produce done. count_ok[3047178] count_fail[26229861]      benchmark_atomic_bit.cpp:46
 * 14 18:10:35.895140 1501564 I thr_c[0] produce done. busy[3148262] idel[136180957]       benchmark_atomic_bit.cpp:70
 * 14 18:10:35.895140 1501567 I thr_p[2] produce done. count_ok[2953219] count_fail[27633389]      benchmark_atomic_bit.cpp:46
 * 1 second
 * thr_p[0] 3047178 + 26229861 = 29,277,039
 * thr_p[1] 1889874 + 20773911 = 22,663,785
 * thr_p[2] 2953219 + 27633389 = 30,586,608
 *          7890271 + 74637161 = 82,527,432
 *
 * thr_c[0] 3148262 + 136180957 = 139,329,219
 * thr_c[1] 2677120 + 147246900 = 149,924,020
 * thr_c[2] 2064890 + 200525970 = 202,590,860
 *          7890272 + 483953827 = 491,844,099
 */
void benchmark_atomic_bit::benchmark_mt(size_t thr_num){
    std::atomic<bool> running{true};
    spd_inf("benchmark atomic_bit ...");
    std::vector<std::jthread> thrs_p;
    std::vector<std::jthread> thrs_c;

    size_t i{0};
    if(thr_num > sizeof(uint64_t)){
        thr_num = 64; // 确保线程数量不会过大
    }
    for(i = 0; i < thr_num; ++i){
        thrs_p.emplace_back([this, i, &running](std::stop_token st){
            size_t count_ok{0};
            size_t count_fail{0};
            size_t index{0};
            spd_inf("thr_p[{}] begin...", i);
            while (running.load(std::memory_order_relaxed)){
                if (_bits.try_pop_lsb(index)) {
                    // 目的是找到空闲线程
                    ++count_ok;
                } else {
                    // 实际项目中，没有尝试空闲线程，采用公平队列分配到某线程的任务队列中；
                    ++count_fail;
                    std::this_thread::yield();
                }
            }
            spd_inf("thr_p[{}] produce done. count_ok[{}] count_fail[{}]", i, CTS(count_ok), CTS(count_fail));
        });

        thrs_c.emplace_back([this, i, &running](std::stop_token st){
            size_t count_busy{0};
            size_t count_idel{0};
            uint64_t state{1};
            atomic_bit<uint64_t>& bits = this->_bits;

            if(!bits.make_lsb_bit(i, state)){
                spd_inf("thr_c[{}] error i is to match", i);
            }
            spd_inf("thr_c[{}] state[{}] begin...", i, state);
            while (running.load(std::memory_order_relaxed)){
                // 实际项目中会轮询自己的fifo队列，不会造成 _bits.test_bit(state) 的情况
                // 只会空想时 _bit.set_bit(state) 一次，竞态概率极地，发生次数也极少；
                if(_bits.test_bit(state)){
                    // 未被生产者清除标记，保持原来标记
                    // 数值越大说明，调度效率越低。
                    ++count_idel;
                    std::this_thread::yield();
                }else{
                    // 被生产清除标记
                    _bits.set_bit(state); // 重新设置标记
                    ++count_busy;
                }
            }
            spd_inf("thr_c[{}] customer done. busy[{}] idel[{}]", i, CTS(count_busy), CTS(count_idel));
        });
    }
    // thr_num producer
    //_bits.pop_lsb();
    
    // thr_num consumer
    // _bits.set_lsb();

    z::sleep_ms(1000);
    running = false;

    // wait for all threads to finish and print statistics
}

void benchmark_atomic_bit::benchmark_mt_noatomic(size_t thr_num){
    std::atomic<bool> running{true};
    spd_inf("benchmark bit not use atomic...");
    std::vector<std::jthread> thrs_p;
    std::vector<std::jthread> thrs_c;

    size_t i{0};
    if(thr_num > sizeof(uint64_t)){
        thr_num = 64; // 确保线程数量不会过大
    }
    for(i = 0; i < thr_num; ++i){
        thrs_p.emplace_back([this, i, &running](std::stop_token st){
            size_t index{0};
            spd_inf("thr_p[{}] begin...", i);
            size_t count_ok{0};
            size_t count_fail{0};
            while (running.load(std::memory_order_relaxed)){
                _bits_natm.pop_lsb(index) ? ++count_ok : ++count_fail;
                //++count_ok;
            }
            spd_inf("thr_p[{}] produce done. count_ok[{}] count_fail[{}]", i, CTS(count_ok), CTS(count_fail));
        });

        thrs_c.emplace_back([this, i, &running](std::stop_token st){
            uint64_t state{1};
            size_t count_busy{0};
            size_t count_idel{0};
            if(!_bits_natm.make_lsb_bit(i, state)){
                spd_inf("thr_c[{}] error i is to match", i);
            }
            //spd_inf("thr_c[{}] state[{}] begin...", i, state);
            while (running.load(std::memory_order_relaxed)){
#if 0
                ++count_busy;
#else
                if(_bits_natm.test_bit(state)){
                    // 未被生产者清除标记，保持原来标记
                    // 数值越大说明，调度效率越低。
                    ++count_idel;
                }else{
                    // 被生产清除标记
                    _bits_natm.set_bit(state); // 重新设置标记
                    ++count_busy;
                }
#endif
            }
            spd_inf("thr_c[{}] customer done. busy[{}] idel[{}]", i, CTS(count_busy), CTS(count_idel));
        });
    }
    // thr_num producer
    //_bits.pop_lsb();
    
    // thr_num consumer
    // _bits.set_lsb();
    for(int i = 0; i < 5; ++i){
        z::sleep_ms(1000);
        if(i == 3){
            running = false;
        }
    }
    // wait for all threads to finish and print statistics
}