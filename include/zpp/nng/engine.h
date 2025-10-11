#pragma once

#include "defs.h"
#include <zpp/namespace.h>
#include <zpp/core/server.h>

NSB_NNG
class engine : public server{
public:
    engine(int argc, char** argv);
    ~engine() override;
public:
    err_t on_timer() override;
    err_t timer() override;
};
NSE_NNG