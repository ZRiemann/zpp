#include "stl_svr.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/system/sleep.h>
#include <zpp/STL/fifo.h>
#include <thread>
#include <stop_token>
#include <zpp/system/time.h>
#include <zpp/system/sleep.h>

USE_ZPP

//#define TEST_FIFO_BASE
#define TEST_1I1O_ST
//#define TEST_1I1O_MT
//#define TEST_SPIN_MIMO_SQUE // 单队列，spin_lock()多线程IO
//#define TEST_ATOMIC_BIT
//#define TEST_CHOOSE_MQUE // 多队列负载均方案

constexpr size_t MAX_COUNT = 1000000000;
constexpr size_t END_COUNT = MAX_COUNT - 1;
constexpr size_t CHUNK_SIZE = 8192;
constexpr size_t CHUNK_CAPACITY = 16;

constexpr size_t MAX_COUNT_1I1O_MT = 1000000000;
constexpr size_t END_COUNT_1I1O_MT = MAX_COUNT_1I1O_MT - 1;

/**
 * @note USE_STOP_TOKEN 测试std::stop_token 对性能的影响
 *  开启 stop_token 测试，使得调度性能降低 16%， 高性能场景不建议使用 stop_token。
 * @note 使用 stop_token
 * consum done. item[1000000000] use 3,608,027 us. 277,000,000 q/s
 * consum done. item[1000000000] use 3,647,319 us. 274,000,000 q/s
 * consum done. item[1000000000] use 3,656,185 us. 273,000,000 q/s
 * @note 不是用 stop_token
 * consum done. item[1000000000] use 3,126,993 us. 319,000,000 q/s
 * consum done. item[1000000000] use 3,031,657 us. 329,000,000 q/s
 * consum done. item[1000000000] use 3,170,689 us. 315,000,000 q/s
 * std::stop_token vs 原子变量：优势与不可替代性分析
 * std::stop_token 和原子变量（如 std::atomic<bool>）作为线程控制机制各有其优势和适用场景。
 * 尽管在高性能场景下原子变量可能更高效，但 std::stop_token 提供了一些独特的功能，使其在特定情况下不可替代。
 * std::stop_token 的优势
 * 结构化线程管理：与 std::jthread 无缝集成，提供统一的线程停止机制，线程结束时自动请求停止，简化资源管理，提供了一种标准化的线程终止模式；
 * 回调机制：支持 std::stop_callback 注册停止请求回调函数，可以在停止请求发生时执行清理操作，而不需要轮询检查
 * std::stop_callback cb(token, []{ 
 *  // 停止请求时执行的清理代码
 * cleanup_resources(); 
 * });
 * 协作式取消的标准实现： 提供了标准库支持的取消机制，减少自定义代码，确保跨库和组件的一致性行为
 * 可组合性：可以将一个 stop_source 连接到多个线程支持复杂的取消传播模式，如取消树或取消组
 * 安全性：设计上避免了竞态条件，线程销毁时自动处理停止请求，提供了线程安全的取消机制
 * 
 * 原子变量的优势
 * 性能：如您的测试所示，在高性能场景下比 stop_token 快约16%，更低的检查开销，特别是使用 memory_order_relaxed
 * 灵活性：可以实现更复杂的状态控制（不仅仅是布尔值），可以与其他原子操作组合使用
 * 更细粒度的内存序控制：可以精确控制内存序（relaxed、acquire、release等），在特定场景下可以进一步优化性能
 * 资源占用：通常比 stop_token 机制占用更少的内存，不需要额外的控制结构
 * 
 *  std::stop_token 的不可替代性
 * 标准化的取消模式：在需要标准一致的取消机制时，stop_token 是首选，特别是在开发库或框架时，提供标准接口更有价值
 * 回调注册能力：std::stop_callback 提供的自动回调机制无法用简单的原子变量实现，对于需要在停止时执行复杂清理的场景非常有价值，与 std::jthread 的集成：
 * 当使用 std::jthread 时，stop_token 提供了自动的生命周期管理，线程销毁时自动请求停止，简化了资源管理
 * 复杂取消传播：在需要将取消信号传播到多个相关组件的场景中更加强大，例如，取消一个任务时需要同时取消其所有子任务
 * 
 * 原子变量的不可替代性
 * 极致性能要求：每纳秒都重要的高性能计算场景中，16%的性能差异可能是决定性的，例如您的测试中，每秒处理3亿次操作的场景
 * 自定义同步原语：当需要构建自定义同步原语时，原子变量提供了必要的底层控制，可以实现更复杂的状态机和协议
 * 精细控制内存序：在需要精确控制内存屏障和可见性的场景中不可替代，特别是在无锁算法实现中
 * 资源受限环境：在内存或资源严格受限的环境中，原子变量的轻量级特性更有优势
 * 
 * 适合使用 std::stop_token 的场景
 * 长时间运行的服务线程：后台服务、监控线程等不需要极致性能的场景，需要优雅关闭的服务组件
 * 复杂的取消传播需求：任务有层次结构，需要取消传播的场景，多个相关线程需要协同停止的情况
 * 需要停止回调的场景：停止时需要执行特定清理操作，资源释放需要按特定顺序进行
 * 标准库集成：与其他使用 std::jthread 的代码集成，开发供他人使用的库或框架
 * 
 * 适合使用原子变量的场景
 * 高性能计算循环：如您的测试中，每秒处理数亿次操作的场景，性能关键的数据处理管道
 * 自定义同步原语：实现自定义锁、屏障或其他同步机制，需要精细控制内存序的场景
 * 资源受限环境：嵌入式系统或资源受限的环境，需要最小化内存占用的场景
 * 特殊状态控制：需要比简单布尔值更复杂的状态控制，多状态转换的并发控制
 */
//#define USE_STOP_TOKEN
/**
 * @brief USE_MAIN_PRODUCER 测试单线程推送，单线程推送效率不足；
 * 14 14:25:42.574695 1473529 I thr[4] consum done. item[1000000000] use 10,338,744 us. 96,000,000 q/s     stl_svr.cpp:95
 * 14 14:25:42.574748 1473526 I thr[1] consum done. item[1000000000] use 10,338,730 us. 96,000,000 q/s     stl_svr.cpp:95
 * 14 14:25:42.574759 1473525 I thr[0] consum done. item[1000000000] use 10,338,772 us. 96,000,000 q/s     stl_svr.cpp:95
 * 14 14:25:42.574770 1473527 I thr[2] consum done. item[1000000000] use 10,338,741 us. 96,000,000 q/s     stl_svr.cpp:95
 * 14 14:25:42.574797 1473528 I thr[3] consum done. item[1000000000] use 10,338,786 us. 96,000,000 q/s     stl_svr.cpp:95
 * @note 对比多线程推送
 * 14 14:28:11.668439 1474101 I thr[3] consum done. item[1000000000] use 3,245,117 us. 308,000,000 q/s     stl_svr.cpp:106
 * 14 14:28:11.731141 1474097 I thr[1] consum done. item[1000000000] use 3,307,884 us. 302,000,000 q/s     stl_svr.cpp:106
 * 14 14:28:11.818687 1474099 I thr[2] consum done. item[1000000000] use 3,395,435 us. 294,000,000 q/s     stl_svr.cpp:106
 * 14 14:28:11.823825 1474103 I thr[4] consum done. item[1000000000] use 3,400,378 us. 294,000,000 q/s     stl_svr.cpp:106
 * 14 14:28:11.891597 1474095 I thr[0] consum done. item[1000000000] use 3,468,357 us. 288,000,000 q/s     stl_svr.cpp:106
 */
//#define USE_MAIN_PRODUCER

#ifdef TEST_ATOMIC_BIT
#include "benchmark_atomic_bit.h"
#endif

// 添加线程亲和性控制选项
#define USE_THREAD_AFFINITY
/**
 * @brief 设置线程亲和性
 * @param thread_id 线程ID
 * @param core_id 要绑定的CPU核心ID
 * @return 是否成功设置
 */
bool set_thread_affinity(pthread_t thread_id, int core_id) {
#ifdef USE_THREAD_AFFINITY
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    int result = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        spd_err("Failed to set thread affinity: {}", strerror(result));
        return false;
    }
    
    // 验证设置是否成功
    CPU_ZERO(&cpuset);
    result = pthread_getaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        spd_err("Failed to get thread affinity: {}", strerror(result));
        return false;
    }
    
    if (!CPU_ISSET(core_id, &cpuset)) {
        spd_err("Thread not assigned to core {}", core_id);
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

/**
 * @brief 获取系统可用的CPU核心数
 * @return CPU核心数
 */
int get_cpu_count() {
    return std::thread::hardware_concurrency();
}


//#include "numa_fifo.h"

#ifdef TEST_1I1O_MT
static void benchmark_1i1o_multi_thr(){
    spd_inf("benchmark 1I1O MT...");
    bool valid{false};
    size_t thr_num{5};
    //std::vector<fifo<size_t>*> fifos; // vec 析构时不能释放指针
    std::vector<std::shared_ptr<fifo<size_t>>> fifos; // 解决指针释放的问题
    std::vector<std::jthread> thrs_p;
    std::vector<std::jthread> thrs_c;

    // 获取系统CPU核心数
    int cpu_count = get_cpu_count();
    spd_inf("System has {} CPU cores", cpu_count);
    
    // 如果CPU核心数不足，调整测试线程数
    if (cpu_count < 10 && cpu_count > 2) { // 需要生产者和消费者各占一个核心
        thr_num = cpu_count / 2;
        spd_inf("Adjusting thread count to {} based on available cores", thr_num);
    }

    size_t i{0};
    for(i = 0; i < thr_num; ++i){
        //fifos.emplace_back(valid, CHUNK_SIZE, CHUNK_CAPACITY); 获取失败
        //fifos.push_back(new fifo<size_t>(valid, CHUNK_SIZE, CHUNK_CAPACITY));
        fifos.push_back(std::make_shared<fifo<size_t>>(valid, CHUNK_SIZE, CHUNK_CAPACITY));
#if !defined USE_MAIN_PRODUCER // 测试在主线程中生产
        thrs_p.emplace_back([&fifos, i, cpu_count](std::stop_token st){
            // 设置生产者线程亲和性 - 使用偶数核心
            pthread_t current_thread = pthread_self();
            int core_id = (i * 2) % cpu_count;
            if (set_thread_affinity(current_thread, core_id)) {
                spd_inf("Producer[{}] bound to CPU core {}", i, core_id);
            }

            fifo<size_t>& que_p = *fifos[i];
            size_t count_fails{0};
            spd_inf("thr[{}] fifo[{}] produce begin on core {}", i, fmt::ptr(&que_p), core_id);
            for(size_t i = 0; i < MAX_COUNT_1I1O_MT; ++i){
                while(!que_p.push(i)){
                    sleep_us(100);
                    ++count_fails;
                }
            }
        spd_inf("thr[{}] produce done. push fail[{}]", i, CTS(count_fails));
        });
#endif
        thrs_c.emplace_back([&fifos, i, cpu_count](std::stop_token st){
             // 设置消费者线程亲和性 - 使用奇数核心
            pthread_t current_thread = pthread_self();
            int core_id = (i * 2 + 1) % cpu_count;
            if (set_thread_affinity(current_thread, core_id)) {
                spd_inf("Consumer[{}] bound to CPU core {}", i, core_id);
            }
            fifo<size_t>& que_p = *fifos[i];
            spd_inf("consume thr[{}] que[{}] on core {}", i, fmt::ptr(&que_p), core_id);
            size_t val;
            size_t cnt{0};
            z::time stop_watch;
            bool run{true};
            size_t count_fails{0};
            while(run){
                if(!que_p.pop(val)){
                    sleep_us(100);
                    ++count_fails;
                    continue;
                }
                if(cnt != val){
                    spd_err("not match cnt[{}] val[{}] diff[{}]", cnt, val, cnt > val ? cnt - val : val -cnt);
                    run = false;
                }

                if(!(val & 0xfffffff)){
                    time_t us = stop_watch.elapsed_us() + 1;
                    spd_inf("consum[{}] {} use {} us. {} q/s pop_fails[{}]",i, CTS(val), CTS(us), CTS((val / us) * 1000000), CTS(count_fails));
                }
                switch(val){
                case 0:
                    spd_inf("thr[{}] fifo[{}] begin consum...", i, fmt::ptr(&que_p));
                    stop_watch.start();
                    break;
                case END_COUNT_1I1O_MT:{
                    time_t us = stop_watch.elapsed_us() + 1;
                    spd_inf("thr[{}] consum done. item[{}] use {} us. {} q/s pop_fails[{}]",i, MAX_COUNT_1I1O_MT, CTS(us), CTS((MAX_COUNT_1I1O_MT / us) * 1000000), count_fails);                
                    run = false;
                    }
                    break;
                default:
                    break;
                }
                ++cnt;

            }
        });
    }
#ifdef USE_MAIN_PRODUCER
    fifo<size_t>& que0 = *fifos[0];
    fifo<size_t>& que1 = *fifos[1];
    fifo<size_t>& que2 = *fifos[2];
    fifo<size_t>& que3 = *fifos[3];
    fifo<size_t>& que4 = *fifos[4];
    for(size_t i = 0; i < MAX_COUNT_1I1O_MT; ++i){
        while(!que0.push(i));
        while(!que1.push(i));
        while(!que2.push(i));
        while(!que3.push(i));
        while(!que4.push(i));
    }
#endif
    //z::sleep(10000); // 不需要sleep
}
#endif
/**
 * @brief 单IO基准测试
 * @note Claude 3.7 AI 结论
 * 典型的基于互斥锁的队列实现通常只能达到几百万到几千万 Q/S
 * 即使是其他优化的无锁队列实现，也很少能超过1亿 Q/S
 * 比大多数基于锁的队列实现快10-100倍
 * 比许多其他无锁队列实现快2-5倍
 * 接近于理论上可能的最大吞吐量（考虑到内存访问和指针操作的硬件限制）
 * 2025.11.14
 * 14 10:30:22.006151 970025 I consum 19338090250240 use 52,661,069,411 us. 367,000,000 q/s        stl_svr.cpp:68
 * 14 10:30:25.010862 970025 I consum 19339163992064 use 52,664,074,124 us. 367,000,000 q/s        stl_svr.cpp:68
 * 14 10:30:27.967273 970025 I consum 19340237733888 use 52,667,030,524 us. 367,000,000 q/s        stl_svr.cpp:68
 * 14 10:30:30.951140 970025 I consum 19341311475712 use 52,670,014,401 us. 367,000,000 q/s        stl_svr.cpp:68
 * 14 10:30:33.890086 970025 I consum 19342385217536 use 52,672,953,347 us. 367,000,000 q/s        stl_svr.cpp:68
 * 
 * 17 10:27:34.725488 1421235 I consum 96462817984512 use 257,609,830,503 us. 374,000,000 q/s	stl_svr.cpp:68
 * 17 10:27:37.629395 1421235 I consum 96463891726336 use 257,612,734,411 us. 374,000,000 q/s	stl_svr.cpp:68

 */
#ifdef TEST_1I1O_ST
static void benchmark_1i1o(){
#if FIFO_BACK_PTR_MODE == 1
    spd_inf("benchmark 1i1o... USE_ATOMIC_BACK_PTR");
#elif FIFO_BACK_PTR_MODE == 2
    spd_inf("benchmark 1i1o... USE_VOLATILE_BACK_PTR");
#else
    spd_inf("benchmark 1i1o... normal BACK_PTR");
#endif
    bool valid{false};
    fifo<size_t> que_p(valid, CHUNK_SIZE, 16);
    if(!valid){
        spd_err("memory insufficient");
        return;
    }

    std::jthread producer([&que_p](std::stop_token token){
        spd_inf("produce begin...");
        size_t push_fails{0};
        for(size_t i = 0; i < MAX_COUNT; ++i){
#ifdef USE_STOP_TOKEN
            if(token.stop_requested()){
                spd_inf("stop token requested. exit thread now.");
                break;
            }
#endif
            while(!que_p.push(i)){
                ++push_fails;
                sleep_us(1000);
            }
            if(!(i & 0x3fffff)){
                spd_inf("push({})", i);
            }
        }
        spd_inf("produce done. push_fails[{}]", CTS(push_fails));
    });

    std::jthread consumer([&que_p](std::stop_token token){
        size_t val;
        size_t cnt{0};
        z::time stop_watch;
        bool run{true};
        size_t count_fails{0};
        while(run){
#ifdef USE_STOP_TOKEN
            if(token.stop_requested()){
                spd_inf("stop token requested. exit thread now.");
                break;
            }
#endif

            if(!que_p.pop(val)){
                sleep_us(1000);
                ++count_fails;
                continue;
            }
            if(cnt != val){
                spd_err("not match cnt[{}] val[{}] diff[{}]", cnt, val, cnt > val ? cnt - val : val -cnt);
                run = false;
            }

            if(!(val & 0xfffffff)){
                time_t us = stop_watch.elapsed_us() + 1;
                spd_inf("consum {} use {} us. {} q/s pop_fails[{}]", val, CTS(us), CTS((val / us) * 1000000), CTS(count_fails));      
            }
            switch(val){
            case 0:
                spd_inf("begin consum...");
                stop_watch.update();
                break;
            case END_COUNT:{
                time_t us = stop_watch.elapsed_us() + 1;
                spd_inf("consum done. item[{}] use {} us. {} q/s", MAX_COUNT, CTS(us), CTS((MAX_COUNT / us) * 1000000), CTS(count_fails));
                run = false;
                }
                break;
            default:
                break;
            }
            ++cnt;

        }
    });
#ifdef USE_STOP_TOKEN
    // 必须sleep(), 否则函数退出时直接触发 stop_token 导致线程不能运行完所有任务
    z::sleep(10000);
#endif
}
#endif
/**
 * @brief 测试队列上限限制
 */
#ifdef TEST_FIFO_BASE
static void test_fifo_capacity_limit(){
    // chunk_size(4), chunk_capacity(2)
    bool valid{false};
    fifo<size_t> que(valid, 4, 2);
    if(!valid){
        spd_err("memory insufficient!");
        return;
    }
    size_t i = 0;
    // 队列上限是4*2 = 8；
    // 队列最多压入到 7，压 8 个时队列满了，无法再压入
    for(; i < 10; i++){
        if(que.not_full()){
            size_t &v = que.back();
            v = i;
            spd_inf("push({})", v);
            que.push();
        }else{
            spd_inf("que is full at item[{}]", i);
            break;
        }
    }

    // 弹出 5 项， 块大小时 4, 会腾出一个块(chunk)
    for(int j = 0; j < 5; ++j){
        if(que.not_empty()){
            size_t &v = que.front();
            spd_inf("pop({})", v);
            que.pop();
        }
    }

    // 继续往里压，腾出的块被复用，可以继续压入4(chunk_size)项
    for(; i < 15; ++i){
        if(que.not_full()){
            size_t &v = que.back();
            v = i;
            spd_inf("push({})", v);
            que.push();
        }else{
            spd_inf("que is full at item[{}]", i);
            break;
        }
    }

    // 测试非0拷贝接口
    size_t val{0};
    while(que.pop(val)){
        spd_inf("pop({})", val);
    }
}
#endif
static void test_fifo(){
#ifdef TEST_FIFO_BASE
    test_fifo_capacity_limit();
#endif
#ifdef TEST_1I1O_ST
    benchmark_1i1o();
#endif
#ifdef TEST_1I1O_MT
    benchmark_1i1o_multi_thr();
#endif
}

stl_svr::stl_svr(int argc, char** argv)
    :server(argc, argv){
}

err_t stl_svr::run(){
    test_fifo();
#ifdef TEST_ATOMIC_BIT
    benchmark_atomic_bit benchmark_bits;
    //benchmark_bits.base();
    benchmark_bits.benchmark_mt(3);
    //benchmark_bits.benchmark_mt_noatomic(3);
#endif
    return ERR_OK;    
}

err_t stl_svr::on_timer(){
    //spd_inf("on timer...");
    return ERR_OK;
}
err_t stl_svr::timer(){
    sleep(1000);
    return ERR_END;
}
#define SVR_NAME stl_svr
#include <zpp/core/main.hpp>