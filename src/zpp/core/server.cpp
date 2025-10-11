/**
 * @file zpp/core/server.cpp
 */
#include <csignal>
#include <iostream>

#include <zpp/json.h>
#include <zpp/spdlog.h>
#include "spdguard.h"
#include <zpp/core/server.h>
#include <zpp/system/tsc.h>
#include <zpp/system/system.h>
#include <zpp/core/monitor.h>

NSB_STATIC
volatile std::sig_atomic_t s_signal_status{0};

void signal_handler(int signal)
{
    s_signal_status = signal;
}

void new_failure_handler()
{
    spd_err("Memory INSUFFICIENT!");
    std::raise(SIGINT);
}

NSE_STATIC

USE_ZPP

server::server(){
    tsc_init();
    
    _spdg = new spdguard;
    _spdg->append_console_sink();
    _spdg->build_sync();
}

server::server(int argc, char** argv)
    :_argc(argc)
    ,_argv(argv){
    _spdg = new spdguard;
    if(argc < 3){
        std::cout<< "usage: " << argv[0] << " <uni-conf.json> <conf_svr_name>" <<std::endl
        << "example: " << argv[0] << " ../doc/config/server.json server" << std::endl;
        return;
    }
    for(int i = 0; i < argc; ++i){
        std::cout << argv[i] << " ";
    }
    std::cout << std::endl;
    
    tsc_init();

    std::set_new_handler(new_failure_handler);
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGILL, signal_handler);
    std::signal(SIGSEGV, signal_handler);

    json conf;
    if(ERR_OK != conf.load_file(argv[1])){
        std::cout << "loading config file: " << argv[1] << " FAILED!" << std::endl;
        exit(-1);
    }else{
        std::cout << "loading config file: " << argv[1] << " Success."<< std::endl;
        bool async{true};
        bool console{true};
        bool rotate_on_open{true};
        std::string name{"server.log"};
        int rotats = 3;
        int max_size = 32;
        json svr_conf(conf);
        json spd_conf(conf);
        if(ERR_OK == conf.get_member(argv[2], svr_conf) &&
            ERR_OK == svr_conf.get_member("spd", spd_conf)){
            spd_conf.get_bool("async", async);
            spd_conf.get_bool("console", console);
            spd_conf.get_bool("rotate_on_open", rotate_on_open);
            spd_conf.get_string("name", name);
            spd_conf.get_int("rotats", rotats);
            spd_conf.get_int("max_size", max_size);
            
        }else{
            std::cout << "Waring: NOT find spd config item: " << argv[2] << std::endl;
        }

        if(console){
            _spdg->append_console_sink();
        }
        _spdg->append_rotating_file(name, max_size, rotats, rotate_on_open);
        async ? _spdg->build_async() : _spdg->build_sync();

        sys::info();
        std::string path, app_name;
        sys::process_path(path, app_name);
        spd_inf("path:{}, name:{};", path, app_name);
        rapidjson::StringBuffer sbuf;
        spd_inf("spd:{}", spd_conf.to_string(sbuf, true));
    }
}

server::~server(){
    monitor_guard::print_statistic();
    delete _spdg;
}

#include <iostream>

err_t server::configure(){
    spd_mark();
    return ERR_OK;
}

err_t server::run(){
    spd_mark();
    return ERR_OK;
}

err_t server::stop(){
    spd_mark();
    return ERR_OK;
}

err_t server::wait_stop(){
    spd_mark();
    return ERR_OK;
}

err_t server::on_timer(){
    return ERR_END;
}
err_t server::timer(){
    return ERR_END;
}

err_t server::handle_signal(int signal){
    return signal != 0 ? ERR_END : ERR_OK;
}

void server::loop(){
    spd_mark();
    for(;;){
        if(ERR_OK != timer()){
            spd_inf("timer exit.");
            break;
        }
        if(ERR_OK != handle_signal(s_signal_status)){
            spd_inf("signal[{}] exit.", s_signal_status);
            break;
        }else if(ERR_OK != on_timer()){
            spd_inf("on_timer exit.");
            break;
        }
    }
}
