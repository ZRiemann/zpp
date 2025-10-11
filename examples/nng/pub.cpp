#include "pub.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/nng/message.h>
#include <zpp/nng/socket.h>
#include <zpp/nng/aio.h>
#include <zpp/system/sleep.h>
#include <zpp/system/time.h>

USE_ZPP


nng_pub::nng_pub(int argc, char** argv)
    :nng::engine(argc, argv){
    spd_inf("nng pub constructor...");
}
nng_pub::~nng_pub(){}

err_t nng_pub::run(){
    nng::socket pub;
    pub.pub0_open();

    return ERR_OK;    
}

err_t nng_pub::stop(){
    return ERR_OK;
}

#define SVR_NAME nng_pub
#include <zpp/core/main.hpp>