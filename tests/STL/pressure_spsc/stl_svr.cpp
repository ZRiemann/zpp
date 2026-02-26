#include "stl_svr.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/system/sleep.h>
#define USE_SPSC 1
#if USE_SPSC
#include <zpp/STL/spsc.hpp>
#else
#include <zpp/STL/fifo.h>
#endif
#include <zpp/STL/ring.hpp>
#include <thread>
#include <stop_token>
#include <zpp/system/time.h>
#include <zpp/system/sleep.h>

USE_ZPP

constexpr size_t MAX_COUNT = 1000000000;
//constexpr size_t MAX_COUNT = 1000000;
constexpr size_t END_COUNT = MAX_COUNT - 1;
constexpr size_t CHUNK_SIZE = 8192;
constexpr size_t CHUNK_CAPACITY = 64;

constexpr size_t RING_CAPACITY = 1 << 20;
constexpr size_t RING_MAX_COUNT = MAX_COUNT;
constexpr size_t RING_END_COUNT = RING_MAX_COUNT - 1;

constexpr size_t PRINT_MASK = 0x7ffffff;


void stl_svr::pressure_spsc(){
#if USE_SPSC
    spd_inf("benchmark SPSC");
    spsc<size_t> que(CHUNK_SIZE, CHUNK_CAPACITY);
#else
#if FIFO_BACK_PTR_MODE == 1
    spd_inf("benchmark 1i1o... USE_ATOMIC_BACK_PTR");
#elif FIFO_BACK_PTR_MODE == 2
    spd_inf("benchmark 1i1o... USE_VOLATILE_BACK_PTR");
#else
    spd_inf("benchmark 1i1o... normal BACK_PTR");
#endif
    bool valid{false};

    fifo<size_t> que(valid, CHUNK_SIZE, 16);
    if(!valid){
        spd_err("memory insufficient");
        return;
    }
#endif
    std::jthread producer([&que](std::stop_token token){
        spd_inf("produce begin...");
        size_t push_fails{0};
        for(size_t i = 0; i < MAX_COUNT; ++i){
            while(!que.push(i)){
                //spd_inf("push {} fail!", i);
                pause(100);
                ++push_fails;
            }
            if(!(i & PRINT_MASK)){
                spd_inf("push:{}, push_fails:{};", CTS(i), CTS(push_fails));
            }
        }
        spd_inf("produce done. push_fails[{}]", CTS(push_fails));
    });

    std::jthread consumer([&que](std::stop_token token){
        size_t val;
        size_t cnt{0};
        z::time stop_watch;
        bool run{true};
        size_t count_fails{0};
        while(run){
            if(!que.pop(val)){
#if 1
                pause(100);
#else
                sleep_ms(1000);
                spd_inf("pop fail! cnt:{}", cnt);
                spd_war("front_ptr:{}, back_ptr:{}, front_end_ptr:{}, back_end_ptr:{}, back_local:{}, back_cached:{}", 
                    fmt::ptr(que._front_ptr), fmt::ptr(que._back_ptr.load(std::memory_order_acquire)), fmt::ptr(que._front_end_ptr), 
                    fmt::ptr(que._back_end_ptr), fmt::ptr(que._back_local), fmt::ptr(que._back_cached));
#endif
                ++count_fails;
                continue;
            }
            if(cnt != val){
                spd_err("not match cnt[{}] val[{}] diff[{}]", cnt, val, cnt > val ? cnt - val : val -cnt);
                spd_err("front_ptr:{}, back_ptr:{}, front_end_ptr:{}, back_end_ptr:{}, back_local:{}, back_cached:{}", 
                    fmt::ptr(que._front_ptr), fmt::ptr(que._back_ptr.load(std::memory_order_acquire)), fmt::ptr(que._front_end_ptr), 
                    fmt::ptr(que._back_end_ptr), fmt::ptr(que._back_local), fmt::ptr(que._back_cached));
                run = false;
            }

            if(!(val & PRINT_MASK)){
                time_t us = stop_watch.elapsed_us() + 1;
                spd_inf("consum {} use {} us. {} q/s pop_fails[{}]", CTS(val), CTS(us), CTS((val / us) * 1000000), CTS(count_fails));      
            }
            switch(val){
            case 0:
                spd_inf("begin consum...");
                stop_watch.update();
                break;
            case END_COUNT:{
                time_t us = stop_watch.elapsed_us() + 1;
                spd_inf("consum done. item[{}] use {} us. {} q/s", CTS(MAX_COUNT), CTS(us), CTS((MAX_COUNT / us) * 1000000), CTS(count_fails));
                run = false;
                }
                break;
            default:
                break;
            }
            ++cnt;

        }
        spd_inf("consume done. pop_fails[{}]", CTS(count_fails));
    });
    spd_inf("pressure spsc test done.");
}

void stl_svr::pressure_ring_compare(){
    //constexpr size_t batches[] = {1, 4, 8, 16};
    constexpr size_t batches[] = {8};

    for(size_t publish_batch : batches){
        spd_inf("benchmark RING(cap:{}, publish_batch:{}, count:{})", CTS(RING_CAPACITY), CTS(publish_batch), CTS(RING_MAX_COUNT));
        ring<size_t> que(RING_CAPACITY, publish_batch);

        std::jthread producer([&que](std::stop_token token){
            size_t push_fails{0};
            for(size_t i = 0; i < RING_MAX_COUNT; ++i){
                while(!que.push(i)){
                    pause(100);
                    ++push_fails;
                }
                if(!(i & PRINT_MASK)){
                    spd_inf("[ring] push:{}, push_fails:{};", CTS(i), CTS(push_fails));
                }
            }
            que.flush();
            spd_inf("[ring] produce done. push_fails[{}]", CTS(push_fails));
        });

        std::jthread consumer([&que](std::stop_token token){
            size_t val;
            size_t cnt{0};
            z::time stop_watch;
            bool run{true};
            size_t pop_fails{0};
            while(run){
                if(!que.pop(val)){
                    pause(100);
                    ++pop_fails;
                    continue;
                }
                if(cnt != val){
                    spd_err("[ring] not match cnt[{}] val[{}] diff[{}]", cnt, val, cnt > val ? cnt - val : val - cnt);
                    run = false;
                }

                if(!(val & PRINT_MASK)){
                    time_t us = stop_watch.elapsed_us() + 1;
                    spd_inf("[ring] consum {} use {} us. {} q/s pop_fails[{}]", CTS(val), CTS(us), CTS((val / us) * 1000000), CTS(pop_fails));
                }

                switch(val){
                case 0:
                    spd_inf("[ring] begin consum...");
                    stop_watch.update();
                    break;
                case RING_END_COUNT:{
                    time_t us = stop_watch.elapsed_us() + 1;
                    spd_inf("[ring] consum done. item[{}] use {} us. {} q/s pop_fails[{}]", CTS(RING_MAX_COUNT), CTS(us), CTS((RING_MAX_COUNT / us) * 1000000), CTS(pop_fails));
                    run = false;
                    }
                    break;
                default:
                    break;
                }
                ++cnt;
            }
            spd_inf("[ring] consume done. pop_fails[{}]", CTS(pop_fails));
        });
    }
}

void stl_svr::basic_spsc(){
    spd_inf("test basic spsc...");
    spsc<std::size_t> que(4, 2);
    for(size_t i = 0; i < 10; ++i){
        if(que.push(i)){
            spd_inf("push {} OK!", i);
        }else{
            spd_err("push {} fail!", i);
        }
    }
}
stl_svr::stl_svr(int argc, char** argv)
    :server(argc, argv){
}

err_t stl_svr::run(){
    //pressure_ring_compare();
    pressure_spsc();
    return ERR_OK;    
}

err_t stl_svr::on_timer(){
    spd_inf("on timer...");
    return ERR_OK;
}
err_t stl_svr::timer(){
    sleep_ms(1000);
    return ERR_END;
}
#define SVR_NAME stl_svr
#include <zpp/core/main.hpp>