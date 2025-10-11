#pragma once

#include <zpp/namespace.h>

NSB_ZPP
class executor_example{
public:
    executor_example() = default;
    ~executor_example() = default;

    void base_example();
private:
    void inline_executor_example();
    void drivable_executor_example();
    void executor_future_example();
    void queued_immediate_executor_example();
    void benchmark_queued_immediate_executor();
};
NSE_ZPP