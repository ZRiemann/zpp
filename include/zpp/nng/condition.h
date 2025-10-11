#pragma once

#include "defs.h"
#include "mutex.h"
NSB_NNG
class condition{
public:
    condition(mutex& mtx){
        nng_cv_alloc(&_cv, mtx._mtx);
    }
    virtual ~condition(){
        nng_cv_free(_cv);
    }
public:
    inline void wait(nng_mtx* mtx){
        nng_cv_wait(_cv, mtx);
    }
public:
    nng_cv* _cv{nullptr};
};
NSE_NNG