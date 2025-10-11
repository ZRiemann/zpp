#pragma once

#include <zpp/core/inspection/namespace.h>
#include <zpp/types.h>
#include <zpp/error.h>

NSB_INSPECTION
template <typename Allocator, typename Graph>
class inspector{
public:
    inspector() = default;
    ~inspector() = default;

    err_t init(const char* name, bool create, size_t thread_count, size_t capacity){
        _graph.init(thread_count, capacity);
        err_t ret = _allocator.init(name, _graph.size(), create);
        if(ERR_OK == ret){
            _graph.attach(_allocator.data());
        }
        return ret;
    }
    void fini(){
        _allocator.fini();
    }
public:
    Allocator _allocator;
    Graph _graph;
};
NSE_INSPECTION