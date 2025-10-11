#pragma once

#include <thread>
#include <vector>
#include <stop_token>
#include <zpp/namespace.h>

NSB_CAMEL
template<typename Scheduler>
class thread_pool{
public:
    thread_pool(){}
    ~thread_pool(){}

    void run(Scheduler& scheduler, size_t thread_count){
        _threads.reserve(thread_count);
        for(size_t i = 0; i < thread_count; ++i){
            _threads.emplace_back(std::jthread([&scheduler, this, i](std::stop_token token){
                while(!token.stop_requested()){
                    scheduler.schedule();
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
NSE_CAMEL