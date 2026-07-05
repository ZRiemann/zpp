/**
 * @brief Application main entry with uni_server subclass
 */
#include <zpp/hpx/server_lifecycle.hpp>
#ifndef SVR_NAME
#include <zpp/hpx/server.hpp>
#endif

#include <hpx/hpx_init.hpp>

NSB_HPX

/**
 * @brief Runs a zpp server inside HPX and finalizes the HPX runtime afterwards.
 * @tparam Server Server type constructible from `(int, char**)`.
 * @param argc Application argument count.
 * @param argv Application argument values.
 * @return The server lifecycle error, or the HPX finalize result on success.
 */
template <typename Server> int run_server(int argc, char **argv) {
  const err_t err = run_server_lifecycle<Server>(argc, argv);
  const int finalize_result = hpx::finalize();
  return ERR_OK == err ? finalize_result : static_cast<int>(err);
}

NSE_HPX

/**
 * @brief Main HPX entry point called by `hpx::init`.
 * @param argc Application argument count.
 * @param argv Application argument values.
 * @return The server or HPX runtime result code.
 */
int hpx_main(int argc, char *argv[]) {
#ifdef SVR_NAME
  return z::zhpx::run_server<SVR_NAME>(argc, argv);
#else
  return z::zhpx::run_server<z::zhpx::server>(argc, argv);
#endif
}

/**
 * @brief Native process entry point that starts the HPX runtime.
 * @param argc Application argument count.
 * @param argv Application argument values.
 * @return The result from `hpx::init`.
 */
int main(int argc, char **argv) { return hpx::init(argc, argv); }
