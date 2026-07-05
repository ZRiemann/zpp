#include <zpp/nng/aio_ctx.h>

#include <utility>

namespace z::nng {

aio_ctx::~aio_ctx() noexcept { close(); }

aio_ctx::aio_ctx(aio_ctx &&other) noexcept
    : ctx_(std::move(other.ctx_)), io_(std::move(other.io_)) {}

aio_ctx &aio_ctx::operator=(aio_ctx &&other) noexcept {
  if (this != &other) {
    close();
    ctx_ = std::move(other.ctx_);
    io_ = std::move(other.io_);
  }
  return *this;
}

int aio_ctx::open(nng_socket owner, callback cb, void *arg) noexcept {
  close();

  int result = ctx_.open(owner);
  if (result != NNG_OK) {
    return result;
  }

  callback_ = cb;
  arg_ = arg;

  const auto completion = cb == nullptr ? nullptr : &aio_ctx::aio_callback;
  result = io_.create(completion, this);
  if (result != NNG_OK) {
    ctx_.close();
    return result;
  }
  return NNG_OK;
}

void aio_ctx::close() noexcept {
  io_.close();
  ctx_.close();
}

bool aio_ctx::valid() const noexcept { return ctx_.valid() && io_.valid(); }

aio_ctx::operator bool() const noexcept { return valid(); }

context &aio_ctx::ctx() noexcept { return ctx_; }

const context &aio_ctx::ctx() const noexcept { return ctx_; }

aio &aio_ctx::io() noexcept { return io_; }

const aio &aio_ctx::io() const noexcept { return io_; }

} // namespace z::nng
