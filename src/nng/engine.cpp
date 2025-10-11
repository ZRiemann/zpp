#include <zpp/nng/engine.h>
#include <zpp/error.h>
#include <zpp/json.h>
#include <zpp/spdlog.h>
#include <zpp/nng/aio.h>

USE_ZPP
USE_NNG

engine::engine(int argc, char** argv)
    :server(argc, argv){
    if(argc < 3){
        spd_war("usage: {} <uni-conf.json> <conf_svr_name>", argv[0]);
        spd_war("example: {} ../doc/config/server.json server", argv[0]);
        return;
    }
    json doc;
    doc.load_file(_argv[1]);
    json conf(doc);
    json conf_engine(doc);
    if (ERR_OK == doc.get_member(_argv[2], conf) &&
        ERR_OK == conf.get_member("nng_engine", conf_engine)) {
        rapidjson::StringBuffer sbuf;
        spd_inf("nng_engine: {}", conf_engine.to_string(sbuf, true));
        // fill nng_init_params from configuration (defaults to 0 == use library defaults)
        nng_init_params params = {0};
        int tmp = 0;

        if (conf_engine.get_int("task_threads", tmp) == ERR_OK) {
            params.num_task_threads = (int16_t)tmp;
        }
        if (conf_engine.get_int("task_threads_max", tmp) == ERR_OK) {
            params.max_task_threads = (int16_t)tmp;
        }
        if (conf_engine.get_int("expire_threads", tmp) == ERR_OK) {
            params.num_expire_threads = (int16_t)tmp;
        }
        if (conf_engine.get_int("expire_threads_max", tmp) == ERR_OK) {
            params.max_expire_threads = (int16_t)tmp;
        }
        if (conf_engine.get_int("poller_threads", tmp) == ERR_OK) {
            params.num_poller_threads = (int16_t)tmp;
        }
        if (conf_engine.get_int("poller_threads_max", tmp) == ERR_OK) {
            params.max_poller_threads = (int16_t)tmp;
        }
        if (conf_engine.get_int("resolver_threads", tmp) == ERR_OK) {
            params.num_resolver_threads = (int16_t)tmp;
        }

        nng_err err = nng_init(&params);
        if (err != NNG_OK) {
            spd_err("nng init failed:{}", nng_strerror(err));
        } else {
            spd_inf("nng init() num_task_threads[{}] max_task_threads[{}] num_expire_threads[{}] max_expire_threads[{}] num_poller_threads[{}] max_poller_threads[{}] num_resolver_threads[{}]",
                params.num_task_threads, params.max_task_threads,
                params.num_expire_threads, params.max_expire_threads,
                params.num_poller_threads, params.max_poller_threads,
                params.num_resolver_threads);
        }
    } else {
        spd_war("Waring: NOT find spd config item:{}.nng_engine", _argv[2]);
    }
}

engine::~engine(){
    spd_inf("max nng::aio refs: {}, cur nng::aio refs: {}", z::nng::aio::max_refs(), z::nng::aio::refs());
    nng_fini();
    spd_inf("nng_fini()");
}

err_t engine::on_timer(){
    return ERR_OK;
}

err_t engine::timer(){
    z::nng::sleep(1000);
    return ERR_OK;
}