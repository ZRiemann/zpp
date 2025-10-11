#pragma once

#include <taskflow/taskflow.hpp>
#include <zpp/system/os.h>
#include <iostream>
#include <thread>

#if Z_OS_LINUX
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif


#include <zpp/namespace.h>
#include <zpp/system/system.h>
#include <zpp/spdlog.h>
#include <zpp/system/tid.h>

NSB_TASKFLOW

class affine_worker : public tf::WorkerInterface {
public:
    affine_worker() = default;
    ~affine_worker() override = default;

    void scheduler_prologue(tf::Worker& w) override {
#if 0 //defined(Z_OS_LINUX) // cause deadlock
        // 1) CPU affinity (Linux)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        unsigned core = w.id() % (std::thread::hardware_concurrency() - 1);
        CPU_SET(core, &cpuset);
        pthread_setaffinity_np(w.thread().native_handle(), sizeof(cpuset), &cpuset);
        spd_inf("[worker {} tid:{}] bound to core:{}", w.id(), z::tid::id(), core);
#else
        spd_inf("[worker {} tid:{}]", w.id(), z::tid::id());
#endif
}

    void scheduler_epilogue(tf::Worker& w, std::exception_ptr ep) override {
        if(ep) try { std::rethrow_exception(ep); }
        catch(const std::exception& e) {
            spd_err("[worker {}] exception: {}", w.id(), e.what());
        }
        spd_inf("[worker {} tid:{}] cleanup", w.id(), z::tid::id());
  }

private:

};

// 用法示例：创建 8 个 worker，base core 从 0 开始，系统有 2 GPUs
// auto wif = tf::make_worker_interface<affine_worker>(0, 2);
// tf::Executor exec(z::sys::physical_cores(), std::move(wif));

NSE_TASKFLOW