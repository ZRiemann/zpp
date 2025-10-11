#pragma once

#include <taskflow/taskflow.hpp>
#include <zpp/namespace.h>
#include <zpp/spdlog.h>
#include <zpp/core/monitor/common_defs.h>
#include <zpp/core/memory/aligned_malloc.hpp>
#include <zpp/system/tsc.h>

#define ENABLE_TIMESTAMPS_TRACE 0

#if ENABLE_TIMESTAMPS_TRACE
#include <zpp/STL/fifo.h>
#endif

NSB_TASKFLOW

/**
 * @class observer_track
 * @brief An observer that tracks worker activity using monitor_guard
 * @note
 *  1. @c _thr_status stores per-thread statistics such as state, task count, and cycle times.
 *  2. @c _taskflows is an array of single-producer-multiple-consumer (spmc) queues that record task timestamps for each worker.
 *  3. The observer hooks into the taskflow execution to update thread states and log task timings.
 */
class observer_worker_status : public tf::ObserverInterface {
public:
    observer_worker_status() = default;
    ~observer_worker_status() override {
        if(_thr_status){
            print_thr_stat(_thr_status, _num_workers);
            aligned_free(_thr_status);
            _thr_status = nullptr;
        }
#if ENABLE_TIMESTAMPS_TRACE
        if(_taskflows){
            for(size_t i = 0; i < _num_workers; ++i){
                delete _taskflows[i];
            }
            aligned_free(_taskflows);
            _taskflows = nullptr;
        }
#endif
    }


    void set_up(size_t num_workers) override final {
        spd_inf("setting up observer_track with {} workers", num_workers);
        _thr_status = static_cast<thr_stat_t*>(aligned_malloc(sizeof(thr_stat_t) * num_workers, alignof(thr_stat_t)));
        if(!_thr_status){
            spd_err("observer_track: failed to allocate memory for thr_stat_t");
            throw std::bad_alloc();
        }
        _num_workers = num_workers;
        std::memset(_thr_status, 0, sizeof(thr_stat_t) * num_workers);
#if ENABLE_TIMESTAMPS_TRACE
        // _taskflows init
        _taskflows = static_cast<spmc<task_timestamps>**>(aligned_malloc(sizeof(spmc<task_timestamps>*) * num_workers, alignof(spmc<task_timestamps>*)));
        for(size_t i = 0; i < num_workers; ++i){
            bool valid{false};
            _taskflows[i] = new spmc<task_timestamps>(valid, 1024, 64);
            if(!valid){
                spd_err("observer_track: failed to create spmc<task_timestamps> for worker {}", i);
                throw std::bad_alloc();
            }
        }
#endif
    }

    void on_entry(tf::WorkerView wv, tf::TaskView tv) noexcept override final {
        thread_local thr_stat_t& ts = _thr_status[wv.id()];
        ts.state = 1;
        if(ts.tasks){
            ts.idel_cycles += z::step_cycles_r(ts.start);
        }else{
            ts.start = z::tsc_now_r();
        }
#if ENABLE_TIMESTAMPS_TRACE
        thread_local spmc<task_timestamps>& _timestamps = *_taskflows[wv.id()];
        // record timestamps
        if(!_timestamps.not_full()){
            // drop the front chunk efficiently when full, if no consumers.
            task_timestamps *first, *end;
            _timestamps.free_chunk(_timestamps.pop_chunk(first, end));
            (void)first;
            (void)end;
        }

        if(_timestamps.not_full()){ // check again
            task_timestamps& tst = _timestamps._fifo.back();
            tst.task_hash = tv.hash_value();
            tst.tsc_start = ts.start;
            tst.tsc_end = 0;
            _timestamps._fifo.push();
        }
#endif
    }

    void on_exit(tf::WorkerView wv, tf::TaskView tv) noexcept override final {
        thread_local thr_stat_t& ts = _thr_status[wv.id()];
        ts.state = 0;
        ts.tasks++;
        ts.task_cycles += z::step_cycles_r(ts.start);
#if ENABLE_TIMESTAMPS_TRACE
        thread_local fifo<task_timestamps>& _timestamps = _taskflows[wv.id()]->_fifo;
        if(_timestamps.not_full()){
            _timestamps.back().tsc_end = ts.start;
        }
#endif
    }

private:
#if ENABLE_TIMESTAMPS_TRACE
    struct task_timestamps{
        uint64_t task_hash;
        tsc_t   tsc_start;
        tsc_t   tsc_end;
    };
    spmc<task_timestamps>** _taskflows{nullptr};
#endif
    size_t _num_workers{0};
    thr_stat_t* _thr_status{nullptr};
};

NSE_TASKFLOW