#include "uni_svr.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/system/sleep.h>
#include "tests.h"
#include <zpp/system/tsc.h>

USE_ZPP

uni_svr::uni_svr(int argc, char** argv)
    :server(argc, argv){
}

err_t uni_svr::run(){
    spd_err("{}", str_err(ERR_TIMEOUT));
    z::test_json();
    z::test_system();
    z::test_timestamp_counter();
    return ERR_OK;    
}

err_t uni_svr::on_timer(){
    //spd_inf("on timer...");
    return ERR_OK;
}
err_t uni_svr::timer(){
    sleep(1000);
    return ERR_END;
}
#define SVR_NAME uni_svr
#include <zpp/core/main.hpp>