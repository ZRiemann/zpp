#include <zpp/hpx/exec/exec.hpp>

#include <iostream>
#include <tuple>

namespace ex = z::hpx::exec;

int main() {
  auto sender = ex::just(21) | ex::then([](int value) { return value * 2; });
  auto result = ex::sync_wait(std::move(sender));
  if (!result) {
    return 1;
  }

  std::cout << std::get<0>(*result) << '\n';
  return 0;
}
