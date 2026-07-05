#pragma once

#include <nng/nng.h>

#include <zpp/core/monitor.h>
#include <zpp/nng/aio.h>
#include <zpp/nng/context.h>
#include <zpp/nng/message.h>

NSB_NNG

/**
 * @brief Movable RAII wrapper for an NNG protocol context and its AIO handle.
 * @note SUB, REQ, REP, SURVEYOR, and RESPONDENT support contexts.
 *       Close every context before stopping its parent protocol.
 */
class aio_ctx {
public:
  aio_ctx() = default;
  ~aio_ctx() noexcept;

  aio_ctx(const aio_ctx &) = delete;
  aio_ctx &operator=(const aio_ctx &) = delete;

  aio_ctx(aio_ctx &&other) noexcept;
  aio_ctx &operator=(aio_ctx &&other) noexcept;

  using callback = void (*)(void *, aio_ctx &, aio::state, nng_err);
  /**
   * @brief Opens a context and allocates its asynchronous I/O handle
   * transactionally.
   * @note A null callback creates a waitable AIO without completion callbacks.
   */
  int open(nng_socket owner, callback cb, void *arg) noexcept;

  /// Stops asynchronous work, frees the AIO, and closes the context.
  void close() noexcept;

  bool valid() const noexcept;
  explicit operator bool() const noexcept;

  /// Returns the owned protocol context wrapper.
  context &ctx() noexcept;
  /// Returns the owned protocol context wrapper.
  const context &ctx() const noexcept;

  aio &io() noexcept;
  const aio &io() const noexcept;

  /**
   * @brief Transfers a message to the AIO and starts an asynchronous send.
   * @note The context must be valid and its AIO must be idle. On failure, use
   *       io().release_msg() to recover the unsent message.
   */
  void send(message &msg) noexcept {
    io_.set_msg(msg.release());
    ctx_.send(io_);
  }
  /**
   * @brief Starts an asynchronous receive on an idle AIO.
   * @note The context must be valid and its AIO must be idle. On failure, use
   *       io().release_msg() to recover the unsent message.
   */
  void send() noexcept { ctx_.send(io_); }

  /// Starts an asynchronous receive on an idle AIO.
  void recv() noexcept { ctx_.recv(io_); }
private:
  context ctx_;
  aio io_;
  callback callback_{nullptr};
  void *arg_{nullptr};

  static void aio_callback(void *arg) noexcept {
    auto *self = static_cast<aio_ctx *>(arg);
    monitor_guard guard;
    self->callback_(self->arg_, *self, self->io_.get_state(),
                    self->io_.result());
  }
};

NSE_NNG
