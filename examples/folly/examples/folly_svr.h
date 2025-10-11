#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>

NSB_ZPP

class folly_svr : public server{
public:
    folly_svr(int argc, char** argv);
    ~folly_svr() override;
    err_t configure() override;
    err_t run() override;
    err_t stop() override;
    err_t on_timer() override;
    err_t timer() override;
};
NSE_ZPP