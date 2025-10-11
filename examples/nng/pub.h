#pragma once

#include <zpp/namespace.h>
#include <zpp/nng/engine.h>

#include <zpp/nng/nng.h>
#include <zpp/nng/aio.h>
NSB_ZPP

class nng_pub : public nng::engine{
public:
    nng_pub(int argc, char** argv);
    ~nng_pub() override;
    err_t run() override;
    err_t stop() override;
};
NSE_ZPP