#pragma once

#include "defs.h"

#include <cstdint>
#include <string>

NSB_NNG

/// RAII owner for an NNG URL object.
class url {
public:
  /// Constructs an empty URL wrapper.
  url() noexcept = default;
  /// Takes ownership of an NNG URL object.
  explicit url(nng_url *value) noexcept;
  /// Frees the owned NNG URL object.
  ~url() noexcept;

  /// URL wrappers cannot be copied because cloning reports NNG errors.
  url(const url &) = delete;
  /// URL wrappers cannot be copied because cloning reports NNG errors.
  url &operator=(const url &) = delete;

  /// Moves ownership from another URL wrapper.
  url(url &&other) noexcept;
  /// Moves ownership from another URL wrapper.
  url &operator=(url &&other) noexcept;

  /// Parses text into an NNG URL and replaces out on success.
  static int parse(const char *text, url &out) noexcept;
  /// Clones an existing URL and replaces out on success.
  static int clone(const url &source, url &out) noexcept;
  /// Parses text into this URL.
  int parse(const char *text) noexcept;
  /// Clones this URL into out.
  int clone(url &out) const noexcept;
  /// Returns true when this wrapper owns an NNG URL object.
  bool valid() const noexcept;
  /// Returns the owned NNG URL object.
  nng_url *get() const noexcept;
  /// Releases ownership of the NNG URL object.
  nng_url *release() noexcept;
  /// Replaces the owned NNG URL object.
  void reset(nng_url *value = nullptr) noexcept;
  /// Swaps two URL wrappers.
  void swap(url &other) noexcept;

  /// Returns the URL scheme without separators.
  const char *scheme() const noexcept;
  /// Returns the URL userinfo field, or nullptr when absent.
  const char *userinfo() const noexcept;
  /// Returns the URL hostname field, or nullptr when absent.
  const char *hostname() const noexcept;
  /// Returns the URL TCP or UDP port in native byte order, or zero when absent.
  std::uint32_t port() const noexcept;
  /// Returns the URL path field, or nullptr when the URL is empty.
  const char *path() const noexcept;
  /// Returns the URL query field without the leading question mark, or nullptr.
  const char *query() const noexcept;
  /// Returns the URL fragment field without the leading hash mark, or nullptr.
  const char *fragment() const noexcept;
  /// Updates a zero port URL with a resolved port.
  void resolve_port(std::uint32_t port) noexcept;
  /// Formats the URL into a string.
  std::string str() const;

private:
  nng_url *url_{nullptr};
};

/// Swaps two URL wrappers.
void swap(url &lhs, url &rhs) noexcept;

NSE_NNG
