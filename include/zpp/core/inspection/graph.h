#pragma once

#include <zpp/core/inspection/namespace.h>
#include <zpp/core/inspection/task.h>
#include <zpp/types.h>

NSB_INSPECTION

/**
 * @brief 单线程状态图
 */
struct thread_status {
  uint64_t state; // 0-idel, 1-busy
  uint64_t tasks; // alltasks handled by this thread

  tsc_t time_point;  // timestamp counter start value (relaxed reads)
  tsc_t task_cycles; // task cost (timestamp counter)
  tsc_t idel_cycles; // idel cost (timestamp counter)

  // uint32_t capacity; // 任务检查点容量，外部记录
  uint32_t index; // 单前任务记入点，循环使用
  task tasks[];   // 任务检查点数据
};

class graph {
public:
  graph() = default;
  ~graph() = default;

  /**
   * @brief 初始化图结构
   * @note 必须在分配器分配内存之前调用
   *  配合 inspection::inspector 使用
   * @param thread_count 线程数量
   * @param capacity 每个线程的任务检查点容量
   */
  inline void init(size_t thread_count, size_t capacity) noexcept {
    thread_count_ = thread_count;
    capacity_ = capacity;
    thread_status_size_ = sizeof(thread_status) + capacity_ * sizeof(task);
  }

  inline void attach(uint8_t *data) noexcept {
    data_ = data;
    for (size_t i = 0; i < thread_count_; ++i) {
      status_[i] =
          reinterpret_cast<thread_status *>(data_ + i * thread_status_size_);
    }
  }
  /**
   * @brief 获取指定线程的状态图指针
   */
  inline thread_status *thread_status_at(size_t thread_id) noexcept {
    if (thread_id >= thread_count_) {
      return nullptr;
    }
    return status_[thread_id];
  }
  /**
   * @brief 计算图的总大小,给分配器使用
   */
  inline size_t size() const noexcept {
    return thread_status_size_ * thread_count_;
  }
  /**
   * @brief 计算单线程状态图大小
   */
  inline size_t thread_status_size() const noexcept {
    return thread_status_size_;
  }
  inline size_t capacity() const noexcept { return capacity_; }

private:
  uint8_t *data_{nullptr};
  thread_status *status_[64]{nullptr};
  size_t capacity_{1024};
  size_t thread_status_size_{0};
  size_t thread_count_{64};
};

NSE_INSPECTION