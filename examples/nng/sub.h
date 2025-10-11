#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>
#include <zpp/nng/nng.h>
NSB_ZPP

class nng_sub : public server{
public:
    nng_sub(int argc, char** argv);
    err_t run() override;

    err_t on_timer() override;
    err_t timer() override;
private:
    nng::nng _nng;
};
NSE_ZPP