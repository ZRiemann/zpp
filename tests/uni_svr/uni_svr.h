#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>

NSB_ZPP

class uni_svr : public server{
public:
    uni_svr(int argc, char** argv);
    err_t run() override;

    err_t on_timer() override;
    err_t timer() override;
private:
};
NSE_ZPP