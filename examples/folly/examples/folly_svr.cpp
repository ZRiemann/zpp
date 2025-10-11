#include "folly_svr.h"
#include <zpp/system/sleep.h>
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include "executor_example.h"
#include <zpp/system/tsc.h>

USE_ZPP

folly_svr::folly_svr(int argc, char** argv)
    :server(argc, argv){
}
folly_svr::~folly_svr(){

}
err_t folly_svr::configure(){
    spd_inf("configure...");
    return ERR_OK;
}
err_t folly_svr::run(){
    spd_inf("running...");
    executor_example ex;
    ex.base_example();
    return ERR_OK;
}
err_t folly_svr::stop(){
    spd_inf("stop...");
    return ERR_OK;
}
err_t folly_svr::on_timer(){
    return ERR_OK;
}
err_t folly_svr::timer(){
    //sleep(1000);
    // return ERR_OK; // timer loop
    return ERR_END; // exit
}

#define SVR_NAME folly_svr
#include <zpp/core/main.hpp>