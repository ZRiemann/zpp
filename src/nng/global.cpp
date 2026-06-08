#include <zpp/nng/aio.h>

std::atomic_int z::nng::aio::refs_{0};
int z::nng::aio::max_refs_{0};