#pragma once

#include "defs.h"
#include <zpp/spdlog.h>

NSB_NNG
/**
 * @class nng
 * @code
 *  int main(){
 *      //nng nng_instance(4, 8, 2, 4, 2, 4, 2);
 *      nng nng_instance; // use default params
 *      return 0;
 *  }
 * @endcode
 */
class nng{
public:
    nng(int16_t num_task_thrs = 0, int16_t max_task_thrs = 0,
        int16_t num_expire_thrs = 0, int16_t max_expire_thrs = 0,
        int16_t num_poller_thrs = 0, int16_t max_poller_thrs = 0,
        int16_t num_resolver_thrs = 0){
        nng_init_params params{0};
	    params.num_task_threads = num_task_thrs;
	    params.max_task_threads = max_task_thrs;
	    params.num_expire_threads = num_expire_thrs;
	    params.max_expire_threads = max_expire_thrs;
    	params.num_poller_threads = num_poller_thrs;
    	params.max_poller_threads = max_poller_thrs;
		params.num_resolver_threads = num_resolver_thrs;
        nng_err err = nng_init(&params);
        spd_inf("nng init[{}]: {}",
            (int)err, nng_strerror(err));
    }
    ~nng(){
        nng_fini();
        spd_inf("nng fini.");
    }
};

NSE_NNG
