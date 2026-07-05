#include <zpp/nng/context.h>

#include <zpp/nng/log.h>

namespace z::nng {

context::context(nng_socket owner) noexcept { (void)open(owner); }

context::context(nng_ctx handle) noexcept : ctx_(handle) {}

context::~context() noexcept { close(); }

context::context(context &&other) noexcept : ctx_(other.release()) {}

context &context::operator=(context &&other) noexcept {
  if (this != &other) {
    reset(other.release());
  }
  return *this;
}

int context::open(nng_socket owner) noexcept {
  close();

  nng_ctx opened{NNG_CTX_INITIALIZER};
  const auto result = nng_ctx_open(&opened, owner);
  if (result != NNG_OK) {
    nng2err(cmp_context, act_create, result);
    return result;
  }

  ctx_ = opened;
  nng2inf(cmp_context, act_create, "id={}", id());
  return result;
}

bool context::valid() const noexcept { return id() > 0; }

context::operator bool() const noexcept { return valid(); }

int context::id() const noexcept { return nng_ctx_id(ctx_); }

nng_ctx context::get() const noexcept { return ctx_; }

nng_ctx context::release() noexcept {
  const auto released = ctx_;
  ctx_ = NNG_CTX_INITIALIZER;
  return released;
}

void context::reset(nng_ctx handle) noexcept {
  if (id() == nng_ctx_id(handle)) {
    return;
  }
  close();
  ctx_ = handle;
}

void context::close() noexcept {
  if (!valid()) {
    return;
  }
  const int context_id = id();
  const auto result = nng_ctx_close(ctx_);
  nng2err(cmp_context, act_close, result);
  if (result == NNG_OK) {
    nng2inf(cmp_context, act_close, "id={}", context_id);
  }
  ctx_ = NNG_CTX_INITIALIZER;
}

int context::set_options(const context_options &options) noexcept {
  auto result = set_ms(NNG_OPT_SENDTIMEO, options.send_timeout_ms);
  if (result != NNG_OK) {
    return result;
  }
  return set_ms(NNG_OPT_RECVTIMEO, options.recv_timeout_ms);
}

int context::get_options(context_options &options) const noexcept {
  context_options result;
  auto error = get_ms(NNG_OPT_SENDTIMEO, &result.send_timeout_ms);
  if (error != NNG_OK) {
    return error;
  }
  error = get_ms(NNG_OPT_RECVTIMEO, &result.recv_timeout_ms);
  if (error == NNG_OK) {
    options = result;
  }
  return error;
}

int context::set_bool(const char *name, bool value) noexcept {
  const auto result = nng_ctx_set_bool(ctx_, name, value);
  nng2opt_err(cmp_context, act_set_bool, result, name);
  return result;
}

int context::get_bool(const char *name, bool *value) const noexcept {
  const auto result = nng_ctx_get_bool(ctx_, name, value);
  nng2opt_err(cmp_context, act_get_bool, result, name);
  return result;
}

int context::set_int(const char *name, int value) noexcept {
  const auto result = nng_ctx_set_int(ctx_, name, value);
  nng2opt_err(cmp_context, act_set_int, result, name);
  return result;
}

int context::get_int(const char *name, int *value) const noexcept {
  const auto result = nng_ctx_get_int(ctx_, name, value);
  nng2opt_err(cmp_context, act_get_int, result, name);
  return result;
}

int context::set_size(const char *name, std::size_t value) noexcept {
  const auto result = nng_ctx_set_size(ctx_, name, value);
  nng2opt_err(cmp_context, act_set_size, result, name);
  return result;
}

int context::get_size(const char *name, std::size_t *value) const noexcept {
  const auto result = nng_ctx_get_size(ctx_, name, value);
  nng2opt_err(cmp_context, act_get_size, result, name);
  return result;
}

int context::set_ms(const char *name, nng_duration value) noexcept {
  const auto result = nng_ctx_set_ms(ctx_, name, value);
  nng2opt_err(cmp_context, act_set_ms, result, name);
  return result;
}

int context::get_ms(const char *name, nng_duration *value) const noexcept {
  const auto result = nng_ctx_get_ms(ctx_, name, value);
  nng2opt_err(cmp_context, act_get_ms, result, name);
  return result;
}

} // namespace z::nng
