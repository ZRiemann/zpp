#pragma once

#include "defs.h"

#include <cstddef>
#include <string_view>

NSB_NNG

class aio;
class url;

/// Runtime options applied to an NNG dialer.
struct dialer_options {
  /// Maximum accepted incoming message size in bytes.
  std::size_t max_recv_size_bytes{1048576};
  /// Minimum reconnect delay in milliseconds.
  nng_duration reconnect_min_ms{1000};
  /// Maximum reconnect delay in milliseconds.
  nng_duration reconnect_max_ms{10000};
};
/**
 * @class dialer
 * @brief RAII owner for an NNG dialer endpoint.
 *
 * @details A dialer can be created, configured, and then started later. This
 * wrapper intentionally stays thin: methods return NNG error codes and log
 * failures with enough endpoint context for diagnosis.
 */
class dialer {
public:
  /// Constructs an empty dialer wrapper.
  dialer() noexcept = default;
  /// Takes ownership of an existing NNG dialer handle.
  explicit dialer(nng_dialer handle) noexcept;
  /// Closes the owned dialer handle.
  ~dialer() noexcept;

  dialer(const dialer &) = delete;
  dialer &operator=(const dialer &) = delete;

  /// Moves ownership from another dialer wrapper.
  dialer(dialer &&other) noexcept;
  /// Moves ownership from another dialer wrapper.
  dialer &operator=(dialer &&other) noexcept;

  /// Returns true when this wrapper owns a valid dialer handle.
  bool valid() const noexcept;
  /// Returns true when this wrapper owns a valid dialer handle.
  explicit operator bool() const noexcept;
  /// Returns the positive NNG dialer id, or -1 for an invalid handle.
  int id() const noexcept;
  /// Returns the underlying NNG dialer handle.
  nng_dialer get() const noexcept;
  /// Releases ownership without closing the dialer handle.
  nng_dialer release() noexcept;
  /// Replaces the owned handle, closing the previous handle first.
  void reset(nng_dialer handle = NNG_DIALER_INITIALIZER) noexcept;
  /// Closes the owned dialer handle and clears this wrapper.
  void close() noexcept;

  /// Creates a dialer from a null-terminated URL string.
  int create(nng_socket socket, const char *target_url) noexcept;
  /// Creates a dialer from a URL string view.
  int create(nng_socket socket, std::string_view target_url);
  /// Creates a dialer from a parsed NNG URL object.
  int create(nng_socket socket, const nng_url *target_url) noexcept;
  /// Creates a dialer from a parsed zpp URL object.
  int create(nng_socket socket, const url &target_url) noexcept;
  /// Starts the dialer.
  int start(int flags = NNG_FLAG_NONBLOCK) noexcept;
  /// Starts the dialer asynchronously and reports the first connection result
  /// through the AIO.
  void start(aio &io, int flags = NNG_FLAG_NONBLOCK) noexcept;

#pragma region Options
  /**
   * @brief Sets optional dialer and TCP transport option groups.
   * @param options Dialer options to apply, or nullptr to skip them.
   * @param transport TCP transport options to apply, or nullptr to skip them.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int set_options(const dialer_options *options,
                  const transport_options *transport) noexcept;
  /**
   * @brief Gets optional dialer and TCP transport option groups.
   * @param options Dialer option output, or nullptr to skip it.
   * @param transport TCP transport option output, or nullptr to skip it.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int get_options(dialer_options *options,
                  transport_options *transport) const noexcept;
  /// Sets a boolean dialer option.
  int set_bool(const char *name, bool value) noexcept;
  /// Gets a boolean dialer option.
  int get_bool(const char *name, bool *value) const noexcept;
  /// Sets an integer dialer option.
  int set_int(const char *name, int value) noexcept;
  /// Gets an integer dialer option.
  int get_int(const char *name, int *value) const noexcept;
  /// Sets a size dialer option.
  int set_size(const char *name, std::size_t value) noexcept;
  /// Gets a size dialer option.
  int get_size(const char *name, std::size_t *value) const noexcept;
  /// Sets a string dialer option.
  int set_string(const char *name, const char *value) noexcept;
  /// Gets a string dialer option.
  int get_string(const char *name, const char **value) const noexcept;
  /// Sets a duration dialer option in milliseconds.
  int set_ms(const char *name, nng_duration value) noexcept;
  /// Gets a duration dialer option in milliseconds.
  int get_ms(const char *name, nng_duration *value) const noexcept;
  /// Sets a socket address dialer option.
  int set_addr(const char *name, const nng_sockaddr *value) noexcept;
  /// Sets the TLS configuration for this dialer.
  int set_tls(nng_tls_config *value) noexcept;
  /// Gets the TLS configuration for this dialer.
  int get_tls(nng_tls_config **value) const noexcept;
  /// Gets the parsed URL associated with this dialer.
  int get_url(const nng_url **value) const noexcept;
#pragma endregion

private:
  nng_dialer dialer_{NNG_DIALER_INITIALIZER};
};

NSE_NNG
