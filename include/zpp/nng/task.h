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
        :aio_(&task<Worker>::aio_handle_cb, this)
        , worker_(worker){
        
        }
    ~task(){
      delete worker_; // used by nng::obj_pool::release()
    }

    /**
     * @brief dispatch the task to nng task queue, and execute in nng task threads
     * @warning if `aio_` not complete dispatch cause fail. Maybe have some design BUGs to FIX
     */
    inline bool dispatch(){
        bool is_ok = aio_.start(task<Worker>::aio_cancel_cb, this);
        if(is_ok){
            aio_.finish(NNG_OK);
        }
        return is_ok;
    }

    inline void handle(){
        nng_err err = aio_.result();
        worker_->work((int)err);
        worker_->release(this);
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
    aio aio_;
    Worker worker_;
};

NSE_NNG
