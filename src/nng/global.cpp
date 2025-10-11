#include <zpp/nng/aio.h>

std::atomic_int z::nng::aio::_refs{0};
int z::nng::aio::_max_refs{0};