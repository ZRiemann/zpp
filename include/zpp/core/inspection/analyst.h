#pragma once

#include <zpp/core/inspection/graph.h>
#include <zpp/core/inspection/namespace.h>
#include <zpp/types.h>

NSB_INSPECTION

/**
 * @brief 根据验视图数据进行分析
 * @note 客户端读取验视图数据后进行分析
 */
class analyst {
public:
  analyst(graph &g) : graph_(g) {}
  ~analyst() = default;

  // void analyze(graph_thread_task* graph, size_t size);
  // virtual void analyze_user(){}

private:
  graph &graph_;
};
NSE_INSPECTION