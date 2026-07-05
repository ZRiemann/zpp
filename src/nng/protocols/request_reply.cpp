#include <zpp/nng/protocols/replier.h>
#include <zpp/nng/protocols/requester.h>

#include <new>
#include <utility>

#include <zpp/core/monitor.h>
#include <zpp/nng/log.h>

USE_NNG
USE_ZPP

namespace {

void log_context_pool_failure(std::string_view operation,
                              const mpmc<aio_ctx *> &pool, const aio_ctx &ctx,
                              std::size_t context_count) noexcept {
  nng2errf(cmp_requester, act_context_pool,
           "operation={} context-id={} contexts={} chunk-size={} chunks={}/{} "
           "reason=push-returned-false",
           operation, ctx.ctx().id(), context_count, pool.chunk_size_,
           pool.chunk_num_.load(std::memory_order_relaxed),
           pool.chunk_capacity_);
}

} // namespace

requester::requester() noexcept : endpoint_protocol(cmp_requester) {}

z::err_t requester::configure(std::vector<endpoint> &&endpoints,
                              const requester_options *requester_config,
                              const socket_options *socket_config,
                              const transport_options *transport_config,
                              const dialer_options *dialer_config,
                              const listener_options *listener_config) {
  const requester_options defaults;
  const auto &selected =
      requester_config == nullptr ? defaults : *requester_config;
  return endpoint_protocol::configure(
      &socket::req0_open, std::move(endpoints), socket_config, transport_config,
      dialer_config, listener_config, &requester::configure_socket, &selected);
}

int requester::configure_socket(socket &target, const void *opaque) {
  const auto &options = *static_cast<const requester_options *>(opaque);
  int result = target.set_ms(NNG_OPT_REQ_RESENDTIME, options.resend_time_ms);
  if (result == NNG_OK) {
    result = target.set_ms(NNG_OPT_REQ_RESENDTICK, options.resend_tick_ms);
  }
  return result;
}

err_t requester::start(std::size_t context_count) {
  const err_t start_result = endpoint_protocol::start();
  if (start_result != ERR_OK) {
    return start_result;
  }

  const std::size_t normalized_count = normalize_concurrency(context_count);
  aio_ctxs.resize(normalized_count);
  for (std::size_t index = 0; index < aio_ctxs.size(); ++index) {
    aio_ctx &ctx = aio_ctxs[index];
    const int result = ctx.open(socket_.get(), &requester::on_event, this);
    if (result != NNG_OK) {
      nng2errf(component_, act_start,
               "failed to open managed context index={} contexts={} error={} "
               "message={}",
               index, normalized_count, result,
               nng_strerror(static_cast<nng_err>(result)));
      stop();
      return result;
    }
  }

  ctx_queue_ = new (std::nothrow) mpmc<aio_ctx *>(aio_ctxs.size() + 1U);
  if (ctx_queue_ == nullptr) {
    nng2errf(component_, act_context_pool,
             "failed to allocate context pool contexts={}", aio_ctxs.size());
    stop();
    return ERR_FAIL;
  }
  for (std::size_t index = 0; index < aio_ctxs.size(); ++index) {
    aio_ctx &ctx = aio_ctxs[index];
    if (!ctx_queue_->push(&ctx)) {
      log_context_pool_failure("initialize", *ctx_queue_, ctx, aio_ctxs.size());
      nng2errf(component_, act_context_pool,
               "failed to initialize context pool index={}", index);
      stop();
      return ERR_FAIL;
    }
  }
  return ERR_OK;
}

void requester::stop() {
  for (auto &ctx : aio_ctxs) {
    if (ctx.io()) {
      ctx.io().stop();
    }
  }
  for (auto &ctx : aio_ctxs) {
    ctx.close();
  }
  delete ctx_queue_;
  ctx_queue_ = nullptr;
  aio_ctxs.clear();
  endpoint_protocol::stop();
}

int requester::request(message &msg, void *hint) noexcept {
  aio_ctx *ctx{nullptr};
  if (!ctx_queue_->pop(ctx)) {
    return NNG_EAGAIN;
  }
  ctx->io().set_hint(hint);
  ctx->send(msg);
  return NNG_OK;
}
void requester::on_reply(message &, nng_err, void *) noexcept {}

void requester::on_receive(aio_ctx &ctx, nng_err result) noexcept {
  if (result != NNG_OK) {
    nng2err(component_, act_recv, result);
  }
  message msg(ctx.io().release_msg());
  on_reply(msg, result, ctx.io().get_hint());
  if (!ctx_queue_->push(&ctx)) {
    log_context_pool_failure("recycle-after-recv", *ctx_queue_, ctx,
                             aio_ctxs.size());
  }
}

void requester::on_send(aio_ctx &ctx, nng_err result) noexcept {
  switch (result) {
  case NNG_OK:
    ctx.recv();
    break;
  default:
    nng2err(component_, act_send, result);
    message release(ctx.io().release_msg());
    on_reply(release, result, ctx.io().get_hint());
    if (!ctx_queue_->push(&ctx)) {
      log_context_pool_failure("recycle-after-send", *ctx_queue_, ctx,
                               aio_ctxs.size());
    }
    break;
  }
}

void requester::on_event(void *arg, aio_ctx &ctx, state state,
                         nng_err result) noexcept {
  requester *self = static_cast<requester *>(arg);
  if (result == NNG_ESTOPPED || result == NNG_ECLOSED ||
      result == NNG_ECANCELED) {
    nng2dbg(self->component_, act_close,
            "managed context stopped id={} state={} result={} message={}",
            ctx.ctx().id(), static_cast<int>(state), static_cast<int>(result),
            nng_strerror(result));
    message release(ctx.io().release_msg());
    self->on_reply(release, result, ctx.io().get_hint());
    return;
  }
  switch (state) {
  case state::init:
    break;
  case state::recv:
    self->on_receive(ctx, result);
    break;
  case state::send:
    self->on_send(ctx, result);
    break;
  default:
    nng2errf(self->component_, act_callback,
             "unexpected AIO state={} context-id={} result={} message={}",
             static_cast<int>(state), ctx.ctx().id(), static_cast<int>(result),
             nng_strerror(result));
    break;
  }
}

//================================================================================
replier::replier() noexcept : endpoint_protocol(cmp_replier) {}

z::err_t replier::configure(std::vector<endpoint> &&endpoints,
                            const socket_options *socket_config,
                            const transport_options *transport_config,
                            const dialer_options *dialer_config,
                            const listener_options *listener_config) {
  return endpoint_protocol::configure(&socket::rep0_open, std::move(endpoints),
                                      socket_config, transport_config,
                                      dialer_config, listener_config);
}

err_t replier::start(std::size_t context_count) {
  const err_t start_result = endpoint_protocol::start();
  if (start_result != ERR_OK) {
    return start_result;
  }

  const std::size_t normalized_count = normalize_concurrency(context_count);
  aio_ctxs.resize(normalized_count);
  for (std::size_t index = 0; index < aio_ctxs.size(); ++index) {
    aio_ctx &ctx = aio_ctxs[index];
    const int result = ctx.open(socket_.get(), &replier::on_event, this);
    if (result != NNG_OK) {
      nng2errf(component_, act_start,
               "failed to open managed context index={} contexts={} error={} "
               "message={}",
               index, normalized_count, result,
               nng_strerror(static_cast<nng_err>(result)));
      stop();
      return result;
    }
  }
  for (aio_ctx &ctx : aio_ctxs) {
    on_event(this, ctx, ctx.io().get_state(), NNG_OK);
  }
  return ERR_OK;
}

void replier::stop() {
  for (auto &ctx : aio_ctxs) {
    if (ctx.io()) {
      ctx.io().stop();
    }
  }
  for (auto &ctx : aio_ctxs) {
    ctx.close();
  }
  aio_ctxs.clear();
  endpoint_protocol::stop();
}

void replier::on_receive(aio_ctx &ctx, nng_err result) noexcept {
  if (result != NNG_OK) {
    nng2err(component_, act_recv, result);
    ctx.recv();
    return;
  }
  message msg(ctx.io().release_msg());
  ctx.send(msg);
}

void replier::on_send(aio_ctx &ctx, nng_err result) noexcept {
  if (result != NNG_OK) {
    nng2err(component_, act_send, result);
    message release(ctx.io().release_msg());
  }
  ctx.recv();
}

void replier::on_event(void *arg, aio_ctx &ctx, state state,
                       nng_err result) noexcept {
  replier *self = static_cast<replier *>(arg);
  if (result == NNG_ESTOPPED || result == NNG_ECLOSED ||
      result == NNG_ECANCELED) {
    nng2dbg(self->component_, act_close,
            "managed context stopped id={} state={} result={} message={}",
            ctx.ctx().id(), static_cast<int>(state), static_cast<int>(result),
            nng_strerror(result));
    message release(ctx.io().release_msg());
    return;
  }
  switch (state) {
  case state::init:
    ctx.recv();
    break;
  case state::recv:
    self->on_receive(ctx, result);
    break;
  case state::send:
    self->on_send(ctx, result);
    break;
  default:
    nng2errf(self->component_, act_callback,
             "unexpected AIO state={} context-id={} result={} message={}",
             static_cast<int>(state), ctx.ctx().id(), static_cast<int>(result),
             nng_strerror(result));
    break;
  }
}
