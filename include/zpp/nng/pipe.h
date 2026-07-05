#pragma once

#include "defs.h"

#include <cstddef>
#include <string>

NSB_NNG

/// Non-owning value wrapper for an NNG pipe handle.
class pipe {
public:
  /// Constructs an invalid pipe handle.
  pipe() noexcept = default;
  /// Wraps an existing NNG pipe handle.
  explicit pipe(nng_pipe p) noexcept;
  /// Destroys the wrapper without closing the pipe.
  ~pipe() = default;

  /// Returns the underlying NNG pipe handle.
  nng_pipe get() const noexcept;
  /// Replaces the wrapped pipe handle.
  void reset(nng_pipe p = NNG_PIPE_INITIALIZER) noexcept;
  /// Returns true when the wrapped pipe has a valid NNG id.
  bool valid() const noexcept;
  /// Returns the NNG pipe id, or -1 for an invalid pipe.
  int id() const noexcept;
  /// Closes the wrapped pipe.
  int close() const noexcept;
  /// Returns the dialer associated with this pipe, if any.
  nng_dialer dialer() const noexcept;
  /// Returns the listener associated with this pipe, if any.
  nng_listener listener() const noexcept;
  /// Returns the socket associated with this pipe.
  nng_socket socket() const noexcept;
  /// Registers a pipe notification callback on this pipe's socket.
  int notify(nng_pipe_ev ev, nng_pipe_cb cb, void *arg) const noexcept;

#pragma region Pipe Addresses
  /// Gets the remote address of this pipe.
  int peer_addr(nng_sockaddr *addr) const noexcept;
  /// Gets the remote address of this pipe.
  int peer_addr(nng_sockaddr &addr) const noexcept;
  /// Gets the local address of this pipe.
  int self_addr(nng_sockaddr *addr) const noexcept;
  /// Gets the local address of this pipe.
  int self_addr(nng_sockaddr &addr) const noexcept;
#pragma endregion
#pragma region Pipe Options
  /// Gets a boolean pipe option.
  int get_bool(const char *opt, bool &val) const noexcept;
  /// Gets an integer pipe option.
  int get_int(const char *opt, int &val) const noexcept;
  /// Gets a duration pipe option in milliseconds.
  int get_ms(const char *opt, nng_duration &val) const noexcept;
  /// Gets a size pipe option.
  int get_size(const char *opt, std::size_t &val) const noexcept;
  /// Gets a borrowed string pipe option.
  int get_string(const char *opt, const char *&val) const noexcept;
  /// Copies a string pipe option into a caller-provided buffer.
  int get_strcpy(const char *opt, char *val, std::size_t len) const noexcept;
  /// Gets a newly allocated string pipe option; free it with nng_strfree.
  int get_strdup(const char *opt, char *&val) const noexcept;
  /// Gets the string length of a pipe option, excluding the terminator.
  int get_strlen(const char *opt, std::size_t &len) const noexcept;
#pragma endregion

#pragma region Helpers
  /// Returns a printable local or remote address for this pipe.
  /// @param peer When true, returns the remote address; otherwise returns the
  /// local address.
  std::string get_address(bool peer) const;
#pragma endregion

public:
  nng_pipe pipe_{NNG_PIPE_INITIALIZER};
};

NSE_NNG
