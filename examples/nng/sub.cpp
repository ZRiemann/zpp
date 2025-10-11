#include "sub.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/system/sleep.h>

USE_ZPP

nng_sub::nng_sub(int argc, char** argv)
    :server(argc, argv)
    ,_nng(){
}

err_t nng_sub::run(){
    return ERR_OK;    
}

err_t nng_sub::on_timer(){
    //spd_inf("on timer...");
    return ERR_OK;
}
err_t nng_sub::timer(){
    sleep(1000);
    return ERR_OK;
}
#define SVR_NAME nng_sub
#include <zpp/core/main.hpp>