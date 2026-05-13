#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <utility>

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

#include <zpp/STL/mpmc.hpp>
#include <zpp/namespace.h>
#include <zpp/spdlog.h>

NSB_TASKFLOW

template<typename T>
class pipeline_mpsc {
public:
    pipeline_mpsc(tf::Executor& executor, tf::Taskflow& taskflow, size_t capacity)
        : _executor(executor), _taskflow(taskflow), _valid(false)
        , _queue(capacity / 2, 2), _is_running(false){}

    virtual ~pipeline_mpsc() {
        wait_done();
    }

    template<typename U>
    bool submit(U&& item) noexcept{
        bool ret = _queue.push(std::forward<U>(item));
        bool expected = false;
        if(_is_running.compare_exchange_strong(expected, true)) {
            _run();            
        }
        return ret;
    }

    bool is_finished() noexcept {
        return !_is_running.load(std::memory_order_acquire)
            && _active_runs.load(std::memory_order_acquire) == 0
            && _queue.empty();
    }

    void wait_done() {
        while(!is_finished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    template<typename Handler>
    auto build_source(Handler&& handler) {
        return tf::Pipe{
            tf::PipeType::SERIAL,
            [this, handler = std::forward<Handler>(handler)](tf::Pipeflow& pf) mutable {
                T data;
                if(_queue.pop(data)){
                    try{
                        handler(pf, std::move(data));
                    }catch(std::exception &e){
                        spd_err("execption:{}", e.what());
                    }catch(...){
                        spd_err("unknown exception");
                    }
                } else {
                    pf.stop();
                }
            }
        };
    }

private:
    void _run() {
        _active_runs.fetch_add(1, std::memory_order_acq_rel);
        _executor.run(_taskflow, [this](){
            if(!_queue.empty()){
                _run();
                _active_runs.fetch_sub(1, std::memory_order_acq_rel);
                return;
            }
            _is_running.store(false, std::memory_order_seq_cst);
            bool expected = false;
            // potential lost wakeup check
            if(!_queue.empty() && _is_running.compare_exchange_strong(expected, true)) {
                _run();
            }
            _active_runs.fetch_sub(1, std::memory_order_acq_rel);
        });
    }

    tf::Executor& _executor;
    tf::Taskflow& _taskflow;
    bool _valid;
    z::mpmc<T> _queue;
    std::atomic<bool> _is_running;
    std::atomic<std::size_t> _active_runs{0};
};

NSE_TASKFLOW
