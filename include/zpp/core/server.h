/**
 * @file modules/server.h
 * @note uniform json config, spdlog, signal handler.
 */
#pragma once

#include <zpp/namespace.h>
#include <zpp/types.h>

/**
 * @class server
 * @note usage
 *  > <server> <conf-json> <server-item>
 *  > server ./server.json server

 * @code {.usage}
#include <zpp/base/config.h>
#include <zpp/modules/server.h>

USE_ZPP
int main(int argc, char **argv){
    err_t err{ERR_FAIL};
    server server(argc, argv);

    do{
        if(!server.conf)break;
        if(ERR_OK != (err = server.configure(server.conf)))break;
        if(ERR_OK != (err = server.run()))break;
        server.pause(5000);
        server.stop();
        server.wait_stop();
        err = ERR_OK;
    }while(0);
    return err;
}
 * @endcode
 */

 NSB_ZPP

class json;
class spdguard;

class server{
public:
    /**
     * @brief Construct a new server object
     * @note just init console spdlog, use by gtest
     */
    server();
    server(int argc, char** argv);
    virtual ~server();

    virtual err_t configure();
    virtual err_t run();
    virtual err_t stop();
    virtual err_t wait_stop();
public:
    virtual err_t on_timer();
    virtual err_t timer();
    virtual err_t handle_signal(int signal);
    void loop();

public:
    int _argc{0}; ///< argv[1] 配置文件路径
    char** _argv{nullptr}; ///< argv[2] 服务配置项目
protected:
    spdguard* _spdg{nullptr};
};
NSE_ZPP
