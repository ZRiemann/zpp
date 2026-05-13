/**
 * @file zpp/core/server.cpp
 */
#include <csignal>
#include <iostream>

#include <zpp/json.hpp>
#include <zpp/spdlog.h>
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

    z::log::config log_conf;
    log_conf.async = false;
    log_conf.console = true;
    log_conf.rotating_file = false;
    z::log::init(log_conf);
}

server::server(int argc, char** argv)
    :_argc(argc)
    ,_argv(argv){
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
        int queue_size = 8192;
        int flush_interval_sec = 2;
        json_view svr_conf;
        json_view spd_conf;
        if(ERR_OK == conf.member(argv[2], svr_conf) &&
            ERR_OK == svr_conf.member("spd", spd_conf)){
            spd_conf.get("async", async);
            spd_conf.get("console", console);
            spd_conf.get("rotate_on_open", rotate_on_open);
            spd_conf.get("name", name);
            spd_conf.get("rotats", rotats);
            spd_conf.get("max_size", max_size);
            spd_conf.get("queue_size", queue_size);
            spd_conf.get("flush_interval_sec", flush_interval_sec);
            
        }else{
            std::cout << "Waring: NOT find spd config item: " << argv[2] << std::endl;
        }

        z::log::config log_conf;
        log_conf.async = async;
        log_conf.console = console;
        log_conf.rotating_file = true;
        log_conf.rotate_on_open = rotate_on_open;
        log_conf.file_name = name;
        log_conf.max_file_size_mb = max_size > 0 ? static_cast<std::size_t>(max_size) : 32;
        log_conf.max_files = rotats > 0 ? static_cast<std::size_t>(rotats) : 3;
        log_conf.queue_size = queue_size > 0 ? static_cast<std::size_t>(queue_size) : 8192;
        log_conf.flush_interval = std::chrono::seconds(flush_interval_sec > 0 ? flush_interval_sec : 0);
        z::log::init(log_conf);

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
    z::log::shutdown();
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
