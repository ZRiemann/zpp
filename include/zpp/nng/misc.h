#pragma once

#include "defs.h"

NSB_NNG

static inline uint32_t random(){
    return nng_random();
}

static inline int socket_pare(int fds[2]){
    return nng_socket_pair(fds);
}

static inline const char *version(){
    return nng_version();
}
NSE_NNG
