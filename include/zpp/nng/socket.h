#pragma once

#include "aio.h"
#include "defs.h"
#include "log.h"
#include "message.h"
#include <cstddef>
#include <cstdint>
#include <zpp/error.h>

NSB_NNG

struct endpoint;
class pipe;

/// Runtime options applied to NNG sockets.
struct socket_options {
  /// Send timeout in milliseconds.
  int send_timeout_ms{100};
  /// Receive timeout in milliseconds.
  int recv_timeout_ms{1000};
  /// Send buffer message count.
  int send_buffer{4096};
  /// Receive buffer message count.
  int recv_buffer{4096};
};

/// RAII wrapper for an NNG socket.
class socket {
public:
#pragma region Constructors and Destructor
  /// Constructs an empty socket.
  socket() = default;
  /// Takes ownership of an already opened NNG socket.
  explicit socket(nng_socket socket);
  /// Closes the owned socket.
  virtual ~socket();

  socket(const socket &) = delete;
  socket &operator=(const socket &) = delete;

  /// Moves a socket.
  socket(socket &&other) noexcept;
  /// Moves a socket.
  socket &operator=(socket &&other) noexcept;

  /// Returns true when the wrapper owns a valid socket.
  bool valid() const;
  /// Returns the underlying socket.
  nng_socket get() const;
  /// Closes and clears the socket.
  void close();
  /// Replaces the owned socket.
  void reset(nng_socket socket);
  /// Releases ownership without closing the socket.
  nng_socket release() noexcept {
    const nng_socket released = sock_;
    sock_ = NNG_SOCKET_INITIALIZER;
    pipe_notify_enabled_ = false;
    return released;
  }
#pragma endregion
#pragma region Socket Identity
  /// Returns the NNG socket id.
  int id() const;

  /// Returns whether the socket is raw.
  int raw(bool *raw);

  /// Returns the protocol id.
  int proto_id(std::uint16_t *idp);

  /// Returns the peer protocol id.
  int peer_id(std::uint16_t *idp);

  /// Returns the protocol name.
  int proto_name(const char **name);

  /// Returns the peer protocol name.
  int peer_name(const char **name);
#pragma endregion
#pragma region Opening a Socket
  /// Opens a BUS0 socket.
  int bus0_open(bool raw = false);
  /// Opens a PUB0 socket.
  int pub0_open(bool raw = false);
  /// Opens a SUB0 socket.
  int sub0_open(bool raw = false);
  /// Opens a PULL0 socket.
  int pull0_open(bool raw = false);
  /// Opens a PUSH0 socket.
  int push0_open(bool raw = false);
  /// Opens a REP0 socket.
  int rep0_open(bool raw = false);
  /// Opens a REQ0 socket.
  int req0_open(bool raw = false);
  /// Opens a RESPONDENT0 socket.
  int respondent0_open(bool raw = false);
  /// Opens a SURVEYOR0 socket.
  int surveyor0_open(bool raw = false);
  /// Opens a PAIR0 socket.
  int pair0_open(bool raw = false);
  /// Opens a PAIR1 socket.
  int pair1_open(bool raw = false);
  /// Opens a polyamorous PAIR1 socket.
  int pair1_open_poly();
#pragma endregion
#pragma region Sending & Receiving Messages
  /// Sends a message and transfers ownership on success.
  int send(message &msg, int flags = NNG_FLAG_NONBLOCK) {
    const auto ret = nng_sendmsg(sock_, msg.msg_, flags);
    if (ret != 0) {
      // ret = NNG_EAGAIN is normal in non-blocking mode
      nng2errf(cmp_socket, act_send, "id={} nng_send failed: {}", id(),
               strerr(ret));
    } else {
      msg.msg_ = nullptr;
    }
    return ret;
  }

  /**
   * @brief asynchronous send on this socket.
   * @note While it does require a few extra steps on the part of the
   * application, the lowest latencies and highest performance will be achieved
   * by using this function instead of nng_send or nng_sendmsg.
   */
  void send(aio &io) {
    io.set_state(aio::state::send);
    nng_socket_send(sock_, io.aio_);
  }

  /// Receives a message into the wrapper.
  int recv(message &msg, int flags = 0) {
    nng_msg *received{nullptr};
    const auto ret = nng_recvmsg(sock_, &received, flags);
    if (ret != 0) {
      // flags = 0 blocks until a message arrives.
      nng2errf(cmp_socket, act_recv, "id={} nng_recv failed: {}", id(),
               strerr(ret));
    } else {
      if (msg.msg_ != nullptr) {
        nng_msg_free(msg.msg_);
      }
      msg.msg_ = received;
    }
    return ret;
  }

  /// Starts asynchronous receive on this socket.
  void recv(aio &io) {
    io.set_state(aio::state::recv);
    nng_socket_recv(sock_, io.aio_);
  }
#pragma endregion
#pragma region Socket Operations
  /**
   * @brief Sets all common socket options.
   * @param options The socket options to apply.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int set_options(const socket_options &options);
  /**
   * @brief Gets all common socket options.
   * @param options The socket options to retrieve.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int get_options(socket_options &options) const;
  /// Sets a boolean socket option.
  int set_bool(const char *name, bool v);

  /// Gets a boolean socket option.
  int get_bool(const char *name, bool *v) const;

  /// Sets an integer socket option.
  int set_int(const char *name, int v);

  /// Gets an integer socket option.
  int get_int(const char *name, int *v) const;

  /// Sets a size socket option.
  int set_size(const char *name, std::size_t v);

  /// Gets a size socket option.
  int get_size(const char *name, std::size_t *v) const;

  /// Sets a duration socket option in milliseconds.
  int set_ms(const char *name, nng_duration v);

  /// Gets a duration socket option in milliseconds.
  int get_ms(const char *name, nng_duration *v) const;
#pragma endregion
#pragma region Pipe Notifications
  /// Starts a listener for the endpoint and removes stale ipc sockets first.
  z::err_t listen(const endpoint &target);
  /// Starts a dialer for the endpoint (non-blocking by default).
  z::err_t dial(const endpoint &target, bool nonblock = true);
  /// Enables or disables NNG pipe event notifications for this socket.
  int pipe_notify(bool enable = true);

  /// Handles a pipe before it is added to the socket; the reference is valid
  /// only during the callback.
  virtual void on_pipe_add_pre(pipe &pipe);
  /// Handles a pipe after it is added to the socket; the reference is valid
  /// only during the callback.
  virtual void on_pipe_add_post(pipe &pipe);
  /// Handles a pipe after it is removed from the socket; the reference is valid
  /// only during the callback.
  virtual void on_pipe_remove_post(pipe &pipe);
#pragma endregion

protected:
  nng_socket sock_{NNG_SOCKET_INITIALIZER};

private:
  static void pipe_event_callback(nng_pipe pipe, nng_pipe_ev event, void *arg);

  int install_pipe_notifications(nng_pipe_cb callback, void *arg);
  void refresh_pipe_notifications_after_move() noexcept;

  bool pipe_notify_enabled_{false};
};

NSE_NNG
