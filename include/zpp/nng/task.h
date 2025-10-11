#pragma once

#include "defs.h"
#include "aio.h"
#include <zpp/types.h>
#include <zpp/core/monitor.h>

NSB_NNG

/**
 * @class task
 * @brief nng engine task template
 * @code
 * class worker{
 * public:
 *      err_t work(){
 *          // wroking int nng task thread
 *          return ERR_OK;
 *      }
 * };
 * 
 * void main(){
 *      worker* w = new worker;
 *      task<worker>* t = new task<worker>(w);
 * 
 *      t.dispatch();
 *      // do someting others...
 * }
 * @endcode
 */
template<typename Worker>
class task{
public:
    task(Worker worker)
        :_aio(&task<Worker>::aio_handle_cb, this)
        , _worker(worker){
        
        }
    ~task(){
      delete _worker; // used by nng::obj_pool::release()
    }

    /**
     * @brief dispatch the task to nng task queue, and execute in nng task threads
     * @warning if `_aio` not complete dispatch cause fail. Maybe have some design BUGs to FIX
     */
    inline bool dispatch(){
        bool is_ok = _aio.start(task<Worker>::aio_cancel_cb, this);
        if(is_ok){
            _aio.finish(NNG_OK);
        }
        return is_ok;
    }

    inline void handle(){
        nng_err err = _aio.result();
        _worker->work((int)err);
        _worker->release(this);
    }
public:
    static void aio_handle_cb(void* h){
        monitor_guard guard;
        ((task<Worker>*)h)->handle();
    }

    static void aio_cancel_cb(nng_aio *aio, void *user, nng_err err){
        nng_aio_finish(aio, err);
    }

public:
    aio _aio;
    Worker _worker;
};

NSE_NNG
