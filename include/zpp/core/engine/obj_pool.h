#pragma once

#include <zpp/namespace.h>
#include <zpp/STL/fifo.h>

NSB_ZPP

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
    obj_pool(bool &valid, size_t chunk_size = 8192, size_t chunk_capacity = 1024)
        :_que(valid, chunk_size, chunk_capacity), _valid(valid){
    }
    ~obj_pool(){
        release();
    }
    /**
     * @brief pop object
     */
    inline bool pop(T*& obj){
        return _que.pop(obj);
    }

    inline bool push(T* obj){
        return _que.push(obj);
    }
    void release(){
        T* obj{nullptr};
        while(_que.pop(obj)){
            delete obj;
        }
    }
public:
    mpmc<T*> _que;
};
NSE_ZPP