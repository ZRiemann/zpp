#pragma once

#include <zpp/core/async/namespace.h>

NSB_ENGINE

/**
 * @brief 静态函数绑定成员函数，适配第三方c接口
 * @note 如nng的aio task threads操作
 */
template <typename T> class binder {
public:
  using fn = void (T::*)();
  binder(T *t, fn f) : t_(t), f_(f) {}

  static inline void async_op(void *arg) {
    binder<T> *b = static_cast<binder<T> *>(arg);
    (b->t_->*(b->f_))();
  }

public:
  T *t_;
  fn f_;
};

#if 0

class aop {
public:
    aop():bind_(this, &aop::fn){
        nng_aio_alloc(&aio_, &binder<aop>::async_op, &bind_);
    }
    
    ~aop(){
        nng_free_aio(aio_);
    }

private:
    binder<aop> bind_;
    nng_aio *aio_;

    void fn() {
        std::cout << "回调被触发!" << std::endl;
    }
};

#endif
NSE_ENGINE