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
        graph_.init(thread_count, capacity);
        err_t ret = allocator_.init(name, graph_.size(), create);
        if(ERR_OK == ret){
            graph_.attach(allocator_.data());
        }
        return ret;
    }
    void fini(){
        allocator_.fini();
    }
public:
    Allocator allocator_;
    Graph graph_;
};
NSE_INSPECTION