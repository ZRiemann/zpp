#pragma once

#include <atomic>
#include <functional>
#include <thread>

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

#include <zpp/namespace.h>
#include <zpp/STL/spsc.hpp>
#include <zpp/system/spin.h>
#include <zpp/spdlog.h>

NSB_TASKFLOW

template<typename T>
class pipeline_mpsc {
public:
    pipeline_mpsc(tf::Executor& executor, tf::Taskflow& taskflow, size_t capacity)
        : _executor(executor), _taskflow(taskflow), _valid(false)
        , _spsc(capacity/2, 2), _is_running(false){}

    virtual ~pipeline_mpsc() {
        wait_done();
    }

    template<typename U>
    bool submit(U&& item) noexcept{
        bool ret{false};

        _spin.lock();
        if(!_spsc.full()){
            _spsc.back() = std::forward<U>(item);
            _spsc.push();
            ret = true;
        }
        _spin.unlock();
        bool expected = false;
        if(_is_running.compare_exchange_strong(expected, true)) {
            _run();            
        }
        return ret;
    }

    bool is_finished() noexcept {
        std::unique_lock<z::spin> lock(_spin);
        return !_is_running.load(std::memory_order_acquire) && _spsc.empty();
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
                if(!_spsc.empty()){
                    try{
                        handler(pf, std::move(_spsc.front()));
                    }catch(std::exception &e){
                        spd_err("execption:{}", e.what());
                    }catch(...){
                        spd_err("unknown exception");
                    }
                    _spsc.pop();
                } else {
                    pf.stop();
                }
            }
        };
    }

private:
    void _run() {
        _executor.run(_taskflow, [this](){
            if(!_spsc.empty()){
                _run();
                return;
            }
            _is_running.store(false, std::memory_order_seq_cst);
            bool expected = false;
            // potential lost wakeup check
            if(!_spsc.empty() && _is_running.compare_exchange_strong(expected, true)) {
                _run();
            }
        });
    }

    tf::Executor& _executor;
    tf::Taskflow& _taskflow;
    bool _valid;
    z::spsc<T> _spsc;
    std::atomic<bool> _is_running;
    mutable z::spin _spin;
};

NSE_TASKFLOW
