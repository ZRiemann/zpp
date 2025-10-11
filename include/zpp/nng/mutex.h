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
        nng_mtx_alloc(&_mtx);
    }
    virtual ~mutex(){
        nng_mtx_free(_mtx);
    }
public:
    inline void lock(){
        nng_mtx_lock(_mtx);
    }
    inline void unlock(){
        nng_mtx_unlock(_mtx);
    }
public:
    nng_mtx* _mtx{nullptr};
};

class guard{
    guard(mutex &m): _mtx(m){
        _mtx.lock();
    }
    ~guard(){
        _mtx.unlock();
    }
private:
    mutex &_mtx;
};

NSE_NNG