#pragma once

#include "defs.h"

#include <cstddef>
#include <string_view>

NSB_NNG

class url;

/// Runtime options applied to an NNG listener.
struct listener_options {
  /// Maximum accepted incoming message size in bytes.
  std::size_t max_recv_size_bytes{1048576};
};
/**
 * @class listener
 * @brief RAII owner for an NNG listener endpoint.
 *
 * @details A listener can be created, configured, and then started later. This
 * wrapper intentionally stays thin: methods return NNG error codes and log
 * failures with enough endpoint context for diagnosis.
 */
class listener {
public:
  /// Constructs an empty listener wrapper.
  listener() noexcept = default;
  /// Takes ownership of an existing NNG listener handle.
  explicit listener(nng_listener handle) noexcept;
  /// Closes the owned listener handle.
  ~listener() noexcept;

  listener(const listener &) = delete;
  listener &operator=(const listener &) = delete;

  /// Moves ownership from another listener wrapper.
  listener(listener &&other) noexcept;
  /// Moves ownership from another listener wrapper.
  listener &operator=(listener &&other) noexcept;

  /// Returns true when this wrapper owns a valid listener handle.
  bool valid() const noexcept;
  /// Returns true when this wrapper owns a valid listener handle.
  explicit operator bool() const noexcept;
  /// Returns the positive NNG listener id, or -1 for an invalid handle.
  int id() const noexcept;
  /// Returns the underlying NNG listener handle.
  nng_listener get() const noexcept;
  /// Releases ownership without closing the listener handle.
  nng_listener release() noexcept;
  /// Replaces the owned handle, closing the previous handle first.
  void reset(nng_listener handle = NNG_LISTENER_INITIALIZER) noexcept;
  /// Closes the owned listener handle and clears this wrapper.
  void close() noexcept;

  /// Creates a listener from a null-terminated URL string.
  int create(nng_socket socket, const char *target_url) noexcept;
  /// Creates a listener from a URL string view.
  int create(nng_socket socket, std::string_view target_url);
  /// Creates a listener from a parsed NNG URL object.
  int create(nng_socket socket, const nng_url *target_url) noexcept;
  /// Creates a listener from a parsed zpp URL object.
  int create(nng_socket socket, const url &target_url) noexcept;
  /// Starts the listener.
  int start(int flags = 0) noexcept;

#pragma region Options
  /**
   * @brief Sets optional listener and TCP transport option groups.
   * @param options Listener options to apply, or nullptr to skip them.
   * @param transport TCP transport options to apply, or nullptr to skip them.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int set_options(const listener_options *options,
                  const transport_options *transport) noexcept;
  /**
   * @brief Gets optional listener and TCP transport option groups.
   * @param options Listener option output, or nullptr to skip it.
   * @param transport TCP transport option output, or nullptr to skip it.
   * @return NNG_OK on success, otherwise an NNG error code.
   */
  int get_options(listener_options *options,
                  transport_options *transport) const noexcept;
  /// Sets a boolean listener option.
  int set_bool(const char *name, bool value) noexcept;
  /// Gets a boolean listener option.
  int get_bool(const char *name, bool *value) const noexcept;
  /// Sets an integer listener option.
  int set_int(const char *name, int value) noexcept;
  /// Gets an integer listener option.
  int get_int(const char *name, int *value) const noexcept;
  /// Sets a size listener option.
  int set_size(const char *name, std::size_t value) noexcept;
  /// Gets a size listener option.
  int get_size(const char *name, std::size_t *value) const noexcept;
  /// Sets a string listener option.
  int set_string(const char *name, const char *value) noexcept;
  /// Gets a string listener option.
  int get_string(const char *name, const char **value) const noexcept;
  /// Sets a duration listener option in milliseconds.
  int set_ms(const char *name, nng_duration value) noexcept;
  /// Gets a duration listener option in milliseconds.
  int get_ms(const char *name, nng_duration *value) const noexcept;
  /// Sets the TLS configuration for this listener.
  int set_tls(nng_tls_config *value) noexcept;
  /// Gets the TLS configuration for this listener.
  int get_tls(nng_tls_config **value) const noexcept;
  /// Sets a platform security descriptor on this listener.
  int set_security_descriptor(void *value) noexcept;
  /// Gets the parsed URL associated with this listener.
  int get_url(const nng_url **value) const noexcept;
#pragma endregion

private:
  nng_listener listener_{NNG_LISTENER_INITIALIZER};
};

NSE_NNG
