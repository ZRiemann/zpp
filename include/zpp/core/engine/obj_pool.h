#pragma once

#include <zpp/STL/fifo.h>
#include <zpp/namespace.h>

NSB_ZPP

/**
 * @brief Be used to avoid new/delete objects. It's thread save and lock free
 * and extremely fast.
 * @note 解决内存碎片问题，提高内存对象的访问和管理效率
 * @code
 * #include <zpp/core/obj_pool.h>
 *
 * static z::obj_pool<int> s_ints;
 *
 * void main(void){
 *      // init obj_pool
 *      for(int i = 0; i < 1024; ++i){
 *          s_ints.push(new int(i));
 *      }
 *      // do something pop/push objects
 *      // ...
 *      s_ints.release(); // delete the objects
 * }
 * @endcode
 */
template <typename T> class obj_pool {
public:
  obj_pool(bool &valid, size_t chunk_size = 8192, size_t chunk_capacity = 1024)
      : que_(valid, chunk_size, chunk_capacity), valid_(valid) {}
  ~obj_pool() { release(); }
  /**
   * @brief pop object
   */
  inline bool pop(T *&obj) { return que_.pop(obj); }

  inline bool push(T *obj) { return que_.push(obj); }
  void release() {
    T *obj{nullptr};
    while (que_.pop(obj)) {
      delete obj;
    }
  }

public:
  mpmc<T *> que_;
};
NSE_ZPP