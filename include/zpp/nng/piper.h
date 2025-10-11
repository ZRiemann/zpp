#pragma once

#include "defs.h"

NSB_NNG
class pipe{
public:
    pipe(){}
    ~pipe(){}

    int id(){
        return nng_pipe_id(_pipe);
    }

    int close(){
        return nng_pipe_close(_pipe);
    }

    nng_dialer dialer(){
        return nng_pipe_dialer(_pipe);
    }
    nng_listener listener(){
        return nng_pipe_listener(_pipe);
    }
    nng_socket socket(){
        return nng_pipe_socket(_pipe);
    }

    /**
     * @brief NNG_PIPE_EV_ADD_PRE, NNG_PIPE_EV_ADD_POST, NNG_PIPE_EV_REM_POST
     */
    int notify(nng_pipe_ev ev, nng_pipe_cb cb, void *arg){
        returan nng_pipe_notify(socket(), ev, cb, arg);
    }
#pragma region Pipe Options
    int get_bool(const char *opt, bool& val){
        return nng_pipe_get_bool(_pipe, opt, &val);
    }
    //...
#pragma endregion
public:
    nng_pipe _pipe{NNG_PIPE_INITIALIZER};
};
NSE_NNG