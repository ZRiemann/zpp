#include <zpp/core/server.h>
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include "coroutine_examples.h"

NSB_APP

USE_ZPP
class server : public z::server{
public:
    server(int argc, char** argv);
    ~server() override;
    err_t run() override;
    err_t stop() override;
};

server::server(int argc, char** argv)
    :z::server(argc, argv){
}
server::~server(){
}
err_t server::run(){
    spd_inf("core server running..."); 
    coroutine_examples ce;
    ce.coro_sleep();
    return ERR_OK;
}
err_t server::stop(){
    spd_inf("core server stopping...");
    return z::server::stop();
}
NSE_APP

#define SVR_NAME app::server
#include <zpp/core/main.hpp>