#pragma once

#include <zpp/types.h>
#include <zpp/core/inspection/task.h>
#include <zpp/core/inspection/namespace.h>

NSB_INSPECTION

/**
 * @brief 单线程状态图
 */
struct thread_status{
    uint64_t state; // 0-idel, 1-busy
    uint64_t tasks; // alltasks handled by this thread

    tsc_t time_point; // timestamp counter start value (relaxed reads)
    tsc_t task_cycles; // task cost (timestamp counter)
    tsc_t idel_cycles; // idel cost (timestamp counter)

    //uint32_t capacity; // 任务检查点容量，外部记录
    uint32_t index;    // 单前任务记入点，循环使用
    task tasks[]; // 任务检查点数据
};

class graph{
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
        _thread_count = thread_count;
        _capacity = capacity;
        _thread_status_size = sizeof(thread_status) + _capacity * sizeof(task);
    }

    inline void attach(uint8_t* data) noexcept {
        _data = data;
        for(size_t i = 0; i < _thread_count; ++i){
            _status[i] = reinterpret_cast<thread_status*>(_data + i * _thread_status_size);
        }
    }
    /**
     * @brief 获取指定线程的状态图指针
     */
    inline thread_status* thread_status_at(size_t thread_id) noexcept {
        if(thread_id >= _thread_count){
            return nullptr;
        }
        return _status[thread_id];
    }
    /**
     * @brief 计算图的总大小,给分配器使用
     */
    inline size_t size() const noexcept {
        return _thread_status_size * _thread_count;
    }
    /**
     * @brief 计算单线程状态图大小
     */
    inline size_t thread_status_size() const noexcept {
        return _thread_status_size;
    }
    inline size_t capacity() const noexcept {
        return _capacity;
    }
private:
    uint8_t* _data{nullptr};
    thread_status* _status[64]{nullptr};
    size_t _capacity{1024};
    size_t _thread_status_size{0};
    size_t _thread_count{64};
};

NSE_INSPECTION