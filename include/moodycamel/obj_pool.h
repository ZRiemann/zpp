#pragma once

#include <zpp/namespace.h>
#include "concurrentqueue.h"

NSB_CAMEL

/**
 * @brief Be used to avoid new/delete objects. It's thread save and lock free and extremely fast.
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
template<typename T>
class obj_pool{
public:
    obj_pool()
        :_queue(){
    }
    ~obj_pool(){
        release();
    }
    /**
     * @brief pop object
     * @note If pop false, user can `new` a object, 
     * and call `push_or_del(obj)` to avoid memory leak.
     * Because T obj construct parameter is unknown,
     * can not call `new T(???)` in the function.
     */
    inline bool pop(T*& obj){
        thread_local moodycamel::ConsumerToken ctoken(_queue);
        if(!_queue.try_dequeue(ctoken, obj)){
            //spd_war("FAILED to pop obj from pool: {} cur_size[{}]", fmt::ptr(obj), _queue.size_approx());
            return false;
        }
        return true;
    }

    inline bool push(T* obj){
        static thread_local moodycamel::ProducerToken ptoken(_queue);
        // don't use `try_enqueue()`, maybe failed!
        // 并不是预先分配了N的对象，pop N个对线，多线程push 回 N 个对象是一定能成功的。中间用了hash算法！
        bool ret = _queue.enqueue(ptoken, obj);
        if(!ret){
            //spd_war("try enqueue failed, delete obj{} size[{}]", fmt::ptr(obj), _queue.size_approx());
            //delete obj; // not delete, obj monitor free
        }
        return ret;
    }
    void release(){
        T* t;
        while(_queue.try_dequeue(t)){
            delete t;
        }
    }
public:
    moodycamel::ConcurrentQueue<T*> _queue;
};
NSE_CAMEL