#pragma once

#include "defs.h"

NSB_NNG

/**
 * @class aio_ctx
 * @note
 * Not all protocols have contexts, because many protocols simply have no state to manage. 
 * The following protocols support contexts: REP, REQ, RESPONDENT, SURVEYOR, SUB
 * @todo use template impliment
 */
class aio_ctx{
public:
    aio_ctx(nng_socket sock, void(*aio_cb)(void*)) {
        nng_ctx_open(&_ctx, sock);
        nng_aio_alloc(&_aio, aio_cb, this);
    }
    ~aio_ctx(){
        nng_ctx_close(_ctx);
        nng_aio_close(_aio);
    }
public:
#if 0
    // aio_ctx reserved
    static inline void aio_cb(nng_aio* aio){
        aio_ctx* ctx = static_cast<aio_ctx*>(nng_aio_get_prov_data(aio));
        ctx->on_aio(aio);
    }
    // 性能考虑直接在回调中处理，不通过虚函数调用
    //virtual void on_aio(nng_aio* aio) = 0;
#endif
public:
    nng_ctx _ctx{NNG_CTX_INITIALIZER};
    nng_aio* _aio{nullptr};
    void *_data{nullptr}; // user data
    enum class state{
        INIT,
        RECV,
        SEND
    } _state{state::INIT};
}
#if 0
void
echo(void *arg)
{
    struct echo_context *ec = arg;

    switch (ec->state) {
    case INIT:
        ec->state = RECV;
        nng_ctx_recv(ec->ctx, ec->aio);
        return;
    case RECV:
        if (nng_aio_result(ec->aio) != 0) {
            // ... handle error
        }
        // We reuse the message on the ec->aio
        ec->state = SEND;
        nng_ctx_send(ec->ctx, ec->aio);
        return;
    case SEND:
        if (nng_aio_result(ec->aio) != 0) {
            // ... handle error
        }
        ec->state = RECV;
        nng_ctx_recv(ec->ctx, ec->aio);
        return;
    }
}
#endif

NSE_NNG