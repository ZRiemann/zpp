#pragma once

#include <zpp/core/inspection/namespace.h>
#include <zpp/core/inspection/graph.h>
#include <zpp/system/tid.h>

NSB_INSPECTION
/**
 * @brief 图数据写入器
 * @note 服务端写入验视图数据
 *  每个线程独立写入自己的线程状态图（无锁设计，预先选择）
 *  每个线程持有一个自己的 writer 实例
 * @note 使用方法：
 *  1. 初始化 graph 结构，分配内存并 attach
 *  2. 每个线程创建一个 writer 实例
 *     使用 `static thread_local` 变量保存 writer 实例，避免重复创建销毁
 *  3. 在线程任务执行过程中，调用 writer 方法写入任务检查点数据
 */
class writer{
public:
    //writer(const writer&) = delete;
    //writer& operator=(const writer&) = delete;
    writer(graph &g):_graph(g), _thread_status(*_graph.thread_status_at(tid::id())) {
    }
    ~writer() = default;

    inline void begin() noexcept {
        
    }
    inline void end() noexcept {
        /*
        uint32_t index = _thread_status->index;
        ++_thread_status->tasks;
        // 记录任务检查点数据
        _thread_status->tasks[index].mission_info = mission_info;
        _thread_status->tasks[index].elapsed = elapsed;

        // 更新下一个记录点索引，循环使用
        _thread_status->index = (++index) % _graph.capacity();
        */
    }
private:
    graph& _graph;
    thread_status& _thread_status;
};
NSE_INSPECTION