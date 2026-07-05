#pragma once

#include <cstddef>

#include <zpp/nng/aio.h>
#include <zpp/nng/defs.h>
#include <zpp/nng/message.h>

NSB_NNG

/// Runtime options shared by all NNG protocol contexts.
struct context_options {
  /// Send timeout in milliseconds.
  nng_duration send_timeout_ms{100};
  /// Receive timeout in milliseconds.
  nng_duration recv_timeout_ms{1000};
};

/**
 * @brief Movable RAII owner for an NNG protocol context.
 * @note SUB, REQ, REP, SURVEYOR, and RESPONDENT support contexts. Close every
 *       context before closing its parent socket.
 */
class context {
public:
  /// Constructs an empty context wrapper.
  context() noexcept = default;
  /// Opens a context owned by the supplied socket.
  explicit context(nng_socket owner) noexcept;
  /// Takes ownership of an existing NNG context handle.
  explicit context(nng_ctx handle) noexcept;
  /// Closes the owned context.
  ~context() noexcept;

  context(const context &) = delete;
  context &operator=(const context &) = delete;

  /// Moves ownership from another context wrapper.
  context(context &&other) noexcept;
  /// Moves ownership from another context wrapper.
  context &operator=(context &&other) noexcept;

  /// Opens a new context, closing the previous context first.
  int open(nng_socket owner) noexcept;
  /// Returns true when this wrapper owns a valid context handle.
  bool valid() const noexcept;
  /// Returns true when this wrapper owns a valid context handle.
  explicit operator bool() const noexcept;
  /// Returns the positive NNG context id, or -1 for an invalid handle.
  int id() const noexcept;
  /// Returns the underlying NNG context handle.
  nng_ctx get() const noexcept;
  /// Releases ownership without closing the context handle.
  nng_ctx release() noexcept;
  /// Replaces the owned context, closing the previous context first.
  void reset(nng_ctx handle = NNG_CTX_INITIALIZER) noexcept;
  /// Closes the owned context and clears this wrapper.
  void close() noexcept;

  /// Sends a message synchronously and transfers ownership on success.
  int send(message &msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    const auto result = nng_ctx_sendmsg(ctx_, msg.msg_, flags);
    if (result == NNG_OK) {
      msg.msg_ = nullptr;
    }
    return result;
  }

  /// Receives a message synchronously into the supplied wrapper.
  int recv(message &msg, int flags = NNG_FLAG_NONBLOCK) noexcept {
    nng_msg *received{nullptr};
    const auto result = nng_ctx_recvmsg(ctx_, &received, flags);
    if (result == NNG_OK) {
      if (msg.msg_ != nullptr) {
        nng_msg_free(msg.msg_);
      }
      msg.msg_ = received;
    }
    return result;
  }

  /// Starts an asynchronous send and records the AIO operation state.
  void send(aio &io) noexcept {
    io.set_state(aio::state::send);
    nng_ctx_send(ctx_, io.get());
  }

  /// Starts an asynchronous receive and records the AIO operation state.
  void recv(aio &io) noexcept {
    io.set_state(aio::state::recv);
    nng_ctx_recv(ctx_, io.get());
  }

  /**
   * @brief Sets all options shared by NNG protocol contexts.
   * @param options The context options to apply.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int set_options(const context_options &options) noexcept;
  /**
   * @brief Gets all options shared by NNG protocol contexts.
   * @param options The context options to retrieve.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int get_options(context_options &options) const noexcept;

  /// Sets a boolean context option.
  int set_bool(const char *name, bool value) noexcept;
  /// Gets a boolean context option.
  int get_bool(const char *name, bool *value) const noexcept;
  /// Sets an integer context option.
  int set_int(const char *name, int value) noexcept;
  /// Gets an integer context option.
  int get_int(const char *name, int *value) const noexcept;
  /// Sets a size context option.
  int set_size(const char *name, std::size_t value) noexcept;
  /// Gets a size context option.
  int get_size(const char *name, std::size_t *value) const noexcept;
  /// Sets a duration context option in milliseconds.
  int set_ms(const char *name, nng_duration value) noexcept;
  /// Gets a duration context option in milliseconds.
  int get_ms(const char *name, nng_duration *value) const noexcept;

private:
  nng_ctx ctx_{NNG_CTX_INITIALIZER};
};

NSE_NNG
