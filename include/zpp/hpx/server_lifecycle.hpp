#pragma once

#include <memory>

#include <zpp/error.h>
#include <zpp/namespace.h>

NSB_HPX

/**
 * @brief Runs the common zpp server lifecycle without owning an HPX runtime.
 * @tparam Server Server type constructible from `(int, char**)`.
 * @param argc Application argument count.
 * @param argv Application argument values.
 * @return The first failing lifecycle error, or `ERR_OK` when all steps
 * succeed.
 */
template <typename Server> err_t run_server_lifecycle(int argc, char **argv) {
  auto server_ptr = std::make_unique<Server>(argc, argv);

  err_t err = server_ptr->configure();
  if (ERR_OK != err) {
    return err;
  }

  err = server_ptr->run();
  if (ERR_OK != err) {
    return err;
  }

  server_ptr->loop();

  err_t result = ERR_OK;
  err_t cleanup_err = server_ptr->stop();
  if (ERR_OK != cleanup_err) {
    result = cleanup_err;
  }

  cleanup_err = server_ptr->wait_stop();
  if (ERR_OK == result && ERR_OK != cleanup_err) {
    result = cleanup_err;
  }

  return result;
}

NSE_HPX
