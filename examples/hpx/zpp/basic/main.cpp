#include <hpx/hpx.hpp>
#include <zpp/error.h>
#include <zpp/hpx/server.hpp>
#include <zpp/spdlog.h>
#include <zpp/system/system.h>

NSB_APP

class server : public z::hpx::server {
public:
  server(int argc, char **argv);
  ~server() override;
  err_t run() override;
  err_t stop() override;
  err_t wait_stop() override;
};

server::server(int argc, char **argv) : z::hpx::server(argc, argv) {
  spd_inf("physical_cores: {}", z::sys::physical_cores());
  spd_inf("hpx_cores: {}", hpx::get_os_thread_count());
}
server::~server() { spd_inf("zpp.hpx.basic server release"); }

err_t server::run() {
  spd_inf("zpp.hpx.basic running...");
  spd_inf("run done.");
  return ERR_OK;
}
err_t server::stop() {
  spd_inf("zpp.hpx.basic stopping...");
  return z::server::stop();
}
err_t server::wait_stop() {
  spd_inf("zpp.hpx.basic waiting for stop...");
  return z::server::wait_stop();
}
NSE_APP

#define SVR_NAME z::app::server
#include <zpp/hpx/main.hpp>
