#pragma once

#include "defs.h"

NSB_NNG

/**
 * @class mutex
 * @brief nng mutex wrapper
 */
class mutex{
public:
    mutex(){
        nng_mtx_alloc(&mtx_);
    }
    virtual ~mutex(){
        nng_mtx_free(mtx_);
    }
public:
    inline void lock(){
        nng_mtx_lock(mtx_);
    }
    inline void unlock(){
        nng_mtx_unlock(mtx_);
    }
public:
    nng_mtx* mtx_{nullptr};
};

class guard{
    guard(mutex &m): mtx_(m){
        mtx_.lock();
    }
    ~guard(){
        mtx_.unlock();
    }
private:
    mutex &mtx_;
};

NSE_NNG