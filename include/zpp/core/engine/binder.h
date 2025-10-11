#pragma once

#include <zpp/core/async/namespace.h>

NSB_ENGINE

/**
 * @brief 静态函数绑定成员函数，适配第三方c接口
 * @note 如nng的aio task threads操作
 */
template<typename T>
class binder{
public:
    using fn = void (T::*)();
    binder(T* t, fn f): _t(t), _f(f){}
    
    static inline void async_op(void *arg) {
        binder<T> *b = static_cast<binder<T>*>(arg);
        (b->_t->*(b->_f))();
    }
    
public:
    T* _t;
    fn _f;
};

#if 0

class aop {
public:
    aop():_bind(this, &aop::fn){
        nng_aio_alloc(&_aio, &binder<aop>::async_op, &_bind);
    }
    
    ~aop(){
        nng_free_aio(_aio);
    }

private:
    binder<aop> _bind;
    nng_aio *_aio;

    void fn() {
        std::cout << "回调被触发!" << std::endl;
    }
};


#endif
NSE_ENGINE