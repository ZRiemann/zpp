#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>

NSB_ZPP

class stl_svr : public server{
public:
    stl_svr(int argc, char** argv);
    err_t run() override;

    err_t on_timer() override;
    err_t timer() override;
private:
    void pressure_spsc();
    void pressure_ring_compare();
    void basic_spsc();
};

NSE_ZPP