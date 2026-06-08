#pragma once

#include "defs.h"
#include "mutex.h"
NSB_NNG
class condition{
public:
    condition(mutex& mtx){
        nng_cv_alloc(&cv_, mtx.mtx_);
    }
    virtual ~condition(){
        nng_cv_free(cv_);
    }
public:
    inline void wait(nng_mtx* mtx){
        nng_cv_wait(cv_, mtx);
    }
public:
    nng_cv* cv_{nullptr};
};
NSE_NNG