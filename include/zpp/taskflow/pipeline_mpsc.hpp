#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <utility>

#include <taskflow/algorithm/pipeline.hpp>
#include <taskflow/taskflow.hpp>

#include <zpp/STL/mpmc.hpp>
#include <zpp/namespace.h>
#include <zpp/spdlog.h>

NSB_TASKFLOW

template <typename T> class pipeline_mpsc {
public:
  pipeline_mpsc(tf::Executor &executor, tf::Taskflow &taskflow, size_t capacity)
      : executor_(executor), taskflow_(taskflow), valid_(false),
        queue_(capacity / 2, 2), is_running_(false) {}

  virtual ~pipeline_mpsc() { wait_done(); }

  template <typename U> bool submit(U &&item) noexcept {
    bool ret = queue_.push(std::forward<U>(item));
    bool expected = false;
    if (is_running_.compare_exchange_strong(expected, true)) {
      _run();
    }
    return ret;
  }

  bool is_finished() noexcept {
    return !is_running_.load(std::memory_order_acquire) &&
           active_runs_.load(std::memory_order_acquire) == 0 && queue_.empty();
  }

  void wait_done() {
    while (!is_finished()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  template <typename Handler> auto build_source(Handler &&handler) {
    return tf::Pipe{tf::PipeType::SERIAL,
                    [this, handler = std::forward<Handler>(handler)](
                        tf::Pipeflow &pf) mutable {
                      T data;
                      if (queue_.pop(data)) {
                        try {
                          handler(pf, std::move(data));
                        } catch (std::exception &e) {
                          spd_err("execption:{}", e.what());
                        } catch (...) {
                          spd_err("unknown exception");
                        }
                      } else {
                        pf.stop();
                      }
                    }};
  }

private:
  void _run() {
    active_runs_.fetch_add(1, std::memory_order_acq_rel);
    executor_.run(taskflow_, [this]() {
      if (!queue_.empty()) {
        _run();
        active_runs_.fetch_sub(1, std::memory_order_acq_rel);
        return;
      }
      is_running_.store(false, std::memory_order_seq_cst);
      bool expected = false;
      // potential lost wakeup check
      if (!queue_.empty() &&
          is_running_.compare_exchange_strong(expected, true)) {
        _run();
      }
      active_runs_.fetch_sub(1, std::memory_order_acq_rel);
    });
  }

  tf::Executor &executor_;
  tf::Taskflow &taskflow_;
  bool valid_;
  z::mpmc<T> queue_;
  std::atomic<bool> is_running_;
  std::atomic<std::size_t> active_runs_{0};
};

NSE_TASKFLOW
