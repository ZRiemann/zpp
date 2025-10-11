#pragma once

#include <zpp/folly/server.h>

NSB_APP

class server : public z::fo::server{
public:
    server(int argc, char** argv);
    ~server() override;
    err_t run() override;
    err_t stop() override;
    err_t on_timer() override;
    err_t timer() override;

private:
    folly::coro::Task<void> mission_cv(std::unique_ptr<folly::IOBuf> buf);
public:
    static server* instance;
};

// `ctx`(global context, avoid pass param deeply) is a convenient alias referring to the global `server::instance` pointer.
// Use an inline(c++17 or later) reference to the static member so `ctx` refers to the same variable. ctx readonly.
inline server* ctx = server::instance;
NSE_APP