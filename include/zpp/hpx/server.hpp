#pragma once

#include <zpp/namespace.h>
#include <zpp/core/server.h>
#include <zpp/error.h>
#include <hpx/hpx.hpp>
#include <chrono>

NSB_HPX
class server : public z::server{
public:
    server(int argc, char** argv):z::server(argc, argv){}
    ~server() override = default;

    /**
     * @brief It keeps the application alive but DOES NOT BLOCK the underlying OS worker thread
     */
    inline err_t timer() override{
        hpx::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return ERR_OK;
    }

    inline err_t on_timer() override{
        return ERR_OK; // continue looping
    }
};
NSE_HPX