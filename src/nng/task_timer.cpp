#include <zpp/nng/task_timer.h>
#include <zpp/spdlog.h>
#include <zpp/error.h>
#include <zpp/core/monitor.h>

USE_ZPP
USE_NNG

task_timer::task_timer(nng_duration d)
    :_aio(&task_timer::aio_handle_cb, this)
    ,_duration(d){

}

task_timer::~task_timer(){

}

void task_timer::aio_handle_cb(void *h){
    monitor_guard stat_gurad;
    task_timer *t = static_cast<task_timer*>(h);
    nng_err err = t->_aio.result();
    err_t ret = t->handle(err);
    if(ret == ERR_OK && err == NNG_OK){
        t->_aio.sleep(t->_duration);
    }// else stop the timer
}

err_t task_timer::handle(nng_err err){
    //spd_inf("on timer...");
    return ERR_OK;
}

void task_timer::run(){
    _aio.sleep(_duration);
}
void task_timer::stop(){
    _aio.stop();
}