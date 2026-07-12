#include <zpp/hpx/exec/exec.hpp>

#include <hpx/init.hpp>

#include <iostream>
#include <tuple>

namespace ex = z::hpx::exec;

int hpx_main() {
  ex::thread_pool_scheduler scheduler;
  auto sender = ex::schedule(scheduler) | ex::then([] { return 42; });
  auto result = ex::sync_wait(std::move(sender));
  if (!result) {
    return hpx::local::finalize();
  }

  std::cout << std::get<0>(*result) << '\n';
  return hpx::local::finalize();
}

int main(int argc, char **argv) {
  hpx::local::init_params init_args;
  init_args.cfg = {"hpx.os_threads=2"};
  return hpx::local::init(hpx_main, argc, argv, init_args);
}
