#include "engine.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <iostream>
#include <zpp/nng/message.h>
#include <zpp/nng/aio.h>
#include <zpp/system/sleep.h>
#include <zpp/system/time.h>
#include <zpp/core/monitor.h>
#include "task_foo.h"
#include <vector>

USE_ZPP

void engine::aio_cancelfn(nng_aio *aio, void *user, nng_err err){
    nng_aio_finish(aio, err);
}

void engine::aio_provider_cb(void* user){
    engine *pub = static_cast<engine*>(user);
    nng::aio& aio = pub->_aio_provider;

    nng_err err = aio.result();
    (void)err;
    spd_inf("nng provider cb... err:[{}]{}", (int)err, nng_strerror(err));
    sleep(100);
}

void engine::test_aio_provider(){
    spd_inf("test aio provider");
    time stw;
    _aio_provider.reset();
    if(_aio_provider.start(&engine::aio_cancelfn, this)){
        _aio_provider.finish(NNG_OK);
    }

    for(int i = 0; i < 3; ++i){
        _aio_provider.wait();
    #if 1
        if(_aio_provider.start(&engine::aio_cancelfn, this)){
            _aio_provider.finish(NNG_OK);
        }else{
            spd_err("try again failed, err:{}",nng_strerror(_aio_provider.result()));
        }
    #endif
    }
    spd_inf("1000 elapsed us[{}]",stw.elapsed_us());
}

void nng_timer_cb(void* user){
    engine *pub = static_cast<engine*>(user);
    nng::aio& aio = pub->_aio_timer;
    spd_inf("nng_timer_cb()...");

    nng_err err = aio.result();
    if(err == NNG_OK || err == NNG_ETIMEDOUT){
        aio.sleep(1000);
    }else{
        spd_err("aio err:[{}]{}", (int)err, nng_strerror(err));
    }
}

engine::engine(int argc, char** argv)
    :nng::engine(argc, argv)
    ,_task_timer(2000){
    spd_inf("nng pub constructor...");
    _aio_timer.create(nng_timer_cb, this);
    _aio_provider.create(aio_provider_cb, this);
}

engine::~engine(){
    task_foo::pool.release();
    monitor_guard::print_statistic();
}

void engine::test_aio_timer(){
    _aio_timer.set_timeout(2000);
    _aio_timer.sleep(1000);
}

void engine::test_msg(){
    nng::message msg(128);
    std::string data{"Hello NNG PUB/SUB World!"};
    msg.append(data.data(), data.size());
    msg.append_u16(2024);
    msg.append_u32(123456789);
    msg.append_u64(9876543210);

    uint64_t u64;
    msg.chop_u64(&u64);
    uint32_t u32;
    msg.chop_u32(&u32);
    uint16_t u16;
    msg.chop_u16(&u16);

    spd_inf("pub created msg: {} {} {} {}", std::string((char*)msg.body(), msg.length()),
        u16, u32, u64);
}

err_t engine::configure(){
    task_foo::init_pool();
    return ERR_OK;
}

err_t engine::run(){
    //test_aio_timer();
    //test_aio_provider();
    //test_msg();
    //_task_timer.run();
    test_task_foo();
    return ERR_OK;    
}

err_t engine::stop(){
    _aio_timer.stop();
    _aio_provider.stop();
    _task_timer.stop();
    return ERR_OK;
}

void engine::test_task_foo(){
#if 1
    // foo working... tasks:10000000 timespan:17837(ms) avg:560000(tasks/sec)
    // total tasks:10000000 tasks_elpased:16022[avg:0] idels_elapsed:565336916[avg:56]
    foo_t* foo{nullptr};
    for(int i = 0; i < task_foo::test_num; ++i){
        if(task_foo::pool.pop(foo)){
            //spd_inf("dispatch foo[{}]", fmt::ptr(foo->_worker));
            foo->dispatch();
        }else{
            --i;
            continue;
        }
    }
#endif
#if 0
    // new/delete benchmark
    // foo working... tasks:10000000 timespan:16573(ms) avg:603000(tasks/sec)
    // total tasks:10000000 tasks_elpased:1050[avg:0] idels_elapsed:665395900[avg:66]      monitor.cpp:29:print_statistic
    std::vector<foo_ptr> foos;
    for(int i = 0; i < task_foo::test_num; ++i){
        foo_ptr t{nullptr};
        task_foo *f{nullptr};
        f = new task_foo();
        t = new foo_t(f);
        foos.push_back(t);
    }
    spd_inf("allock done...");
    for(auto& f : foos){
        f->dispatch();
    }
    spd_inf("dispatch done...");
#endif
}

#define SVR_NAME engine
#include <zpp/core/main.hpp>