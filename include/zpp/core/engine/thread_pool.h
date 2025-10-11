#pragma once

#include <thread>
#include <vector>
#include <stop_token>
#include <zpp/namespace.h>
#include <zpp/core/monitor.h>
#include <zpp/spdlog.h>
#include <zpp/system/time.h>
#include <mutex>
#include <zpp/system/sleep.h>
#include <zpp/system/tid.h>

NSB_ZPP
/**
 * @brief 基于fifo的线程池实现，配合aop
 * @note benchmark 3P3C 单独生产者线程，线程池时消费者不生产
 * 26 20:12:20.182786 609518 I system start: 2025-11-26 20:10:59.473250 time span: 80 sec  monitor.cpp:9
 * 26 20:12:20.183134 609518 I thr[0] IDEL:78,012,508,905(ns) handle_tasks:10,002,184 tasks_elpased:728,520,154(ns) idels_elapsed:908,219,358(ns) Q/S:6,111,042    monitor.cpp:18
 * 26 20:12:20.183504 609518 I thr[1] IDEL:78,009,717,041(ns) handle_tasks:9,997,859 tasks_elpased:1,086,818,090(ns) idels_elapsed:576,049,177(ns) Q/S:6,012,421   monitor.cpp:18
 * 26 20:12:20.183824 609518 I thr[2] IDEL:78,012,955,705(ns) handle_tasks:9,999,957 tasks_elpased:1,142,630,363(ns) idels_elapsed:522,100,487(ns) Q/S:6,006,951   monitor.cpp:18
 * 26 20:12:20.184080 609518 I total tasks:30,000,000 tasks_elpased:2,957,968,607ns [avg:98 ns/q] idels_elapsed:2,006,369,022ns [avg:66 ns/q] Q/S[6,043,102]       monitor.cpp:28 
 * @note benchmark 3P3C 消费者消费后自己生产
 * 27 16:18:08.153887 692967 I thr[0] BUSY:78,961(ns) handle_tasks:167,551,209 tasks_elpased:22,024,068,069(ns) idels_elapsed:17,513,507,373(ns) Q/S:4,237,771     monitor.cpp:18
 * 27 16:18:08.153970 692967 I thr[1] IDEL:158,953(ns) handle_tasks:161,477,721 tasks_elpased:27,575,493,553(ns) idels_elapsed:9,192,613,925(ns) Q/S:4,391,787     monitor.cpp:18
 * 27 16:18:08.154023 692967 I thr[2] IDEL:207,232(ns) handle_tasks:161,561,113 tasks_elpased:26,365,160,473(ns) idels_elapsed:10,856,825,745(ns) Q/S:4,340,475    monitor.cpp:18
 * 27 16:18:08.154068 692967 I total tasks:490,590,043 tasks_elpased:75,964,722,095ns [avg:154 ns/q] idels_elapsed:37,562,947,043ns [avg:76 ns/q] Q/S[4,321,325]   monitor.cpp:28
 * @note benchmark 1P3C 单个任务，线程池自消费自生产
 * 27 16:25:03.365334 693939 I thr[0] IDEL:59,601(ns) handle_tasks:13,964,624 tasks_elpased:8,586,474,257(ns) idels_elapsed:60,968,643,313(ns) Q/S:200,770 monitor.cpp:18
 * 27 16:25:03.365393 693939 I thr[1] IDEL:34,357(ns) handle_tasks:13,964,524 tasks_elpased:9,293,275,170(ns) idels_elapsed:60,169,620,846(ns) Q/S:201,035 monitor.cpp:18
 * 27 16:25:03.365459 693939 I thr[2] IDEL:72,483,001(ns) handle_tasks:303 tasks_elpased:262,181,071(ns) idels_elapsed:69,769,738,604(ns) Q/S:4    monitor.cpp:18
 * 27 16:25:03.365516 693939 I total tasks:27,929,451 tasks_elpased:18,141,930,498ns [avg:649 ns/q] idels_elapsed:190,908,002,763ns [avg:6835 ns/q] Q/S[133,601]   monitor.cpp:28
 */
template<typename T>
class thread_pool{
public:
    thread_pool(){}
    ~thread_pool(){}

    template<typename Scheduler>
    void run(Scheduler& scheduler){
        _threads.reserve(scheduler._thr_num);
        for(size_t i = 0; i < scheduler._thr_num; ++i){
            _threads.emplace_back(std::jthread([&scheduler, this, i](std::stop_token token){
                T* t{nullptr};
                auto &que = *scheduler._ctxs[i].que;
                auto &mtx = scheduler._ctxs[i].mtx;
                auto &cv = scheduler._ctxs[i].cv;
                // ret true: que.not_empty() may be notify maby timeout; false: timeout and que is empty!
                //bool ret{false}; // ret = cv.wait_for(...)
                uint64_t idel_bit;
                scheduler._idels.make_lsb_bit(i, idel_bit);
                z::time stopwatch;
                int tid = tid::id();
                scheduler._ctxs[i].tid = tid;
                spd_inf("consumer[{}] running...", tid);
                for(;;){
                    scheduler._idels.set_bit(idel_bit); // 标记空闲
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait_for(lock, token, std::chrono::microseconds(scheduler._timeout),[&que]{ return que.not_empty(); });
                    //cv.wait(lock, token,[&que]{ return que.not_empty(); }); // 会卡死
                    if (!token.stop_requested()) {
                        lock.unlock();
                        while(que.pop(t)){
                            z::monitor_guard guard; // 如果内部自己维护统计，将无法检测到线程池内部线程被卡死
                            t->operate();
                        }
                    }else{
                        // stop requested
                        spd_inf("thr[{}] stop requested. exit thread now.", tid);
                        break;
                    }
                }
            }));
        }
    }

    void stop(){
        for(auto& thread : _threads){
            thread.request_stop();
        }
    }
private:
    std::vector<std::jthread> _threads;
};
NSE_ZPP