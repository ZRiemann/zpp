#pragma once

#include <zpp/namespace.h>
#include <zpp/moodycamel/concurrentqueue.h>
#include <zpp/core/monitor.h>

NSB_CAMEL
/**
 * @brief aop 调度器
 */
template<typename T>
class scheduler{
public:
    scheduler() = default;
    ~scheduler() = default;
    /**
     * @brief async operator producer
     */
    bool dispatch(T* t){
        static thread_local moodycamel::ProducerToken ptoken(_queue);
        return _queue.enqueue(ptoken, t);
    }
    /**
     * @brief async opertor consumer
     */
    void schedule(){
        T* t;
        thread_local moodycamel::ConsumerToken ctoken(_queue);
        if(!_queue.try_dequeue(ctoken, t)){
            std::this_thread::sleep_for(std::chrono::microseconds(_sleep_us));
            return;
        }
        do{
            monitor_guard mon_guard;
            t->operate();
        }while(_queue.try_dequeue(ctoken, t));
    }
public:
    int _sleep_us{100};
    moodycamel::ConcurrentQueue<T*> _queue;
};
NSE_CAMEL