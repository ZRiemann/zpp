#pragma once

#include <zpp/namespace.h>
#include <zpp/nng/engine.h>

#include <zpp/nng/nng.h>
#include <zpp/nng/aio.h>
#include <zpp/nng/task_timer.h>
NSB_ZPP

class engine : public nng::engine{
public:
    engine(int argc, char** argv);
    ~engine() override;

    err_t configure() override;
    err_t run() override;
    err_t stop() override;

private:
    void test_msg();
    void test_aio_timer();
    void test_task_foo();
public:
    nng::aio _aio_timer;
    nng::task_timer _task_timer;

public: // test I/O provider
    // Most applications will not need to use nng_aio_get_input or nng_aio_set_output, 
    // as those are used by I/O Providers.
    nng::aio _aio_provider;
    static void aio_provider_cb(void* user);
    static void aio_cancelfn(nng_aio *, void *, nng_err);
    void test_aio_provider();
};
NSE_ZPP