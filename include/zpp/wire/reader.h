#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include <zpp/wire/detail/endian.h>

namespace z::wire {

/// Reads little-endian scalar values and raw bytes from a fixed-size buffer.
class reader {
public:
  /// Creates a reader over the supplied immutable buffer.
  explicit reader(std::span<const std::byte> buffer) noexcept
      : current_(buffer.data()), capacity_(buffer.size()),
        remaining_(buffer.size()) {}

  /// Reads one raw byte.
  bool read_byte(std::byte &value) noexcept {
    if (!ensure_available(1)) {
      return false;
    }
    value = *current_++;
    --remaining_;
    return true;
  }

  /// Reads an unsigned 16-bit little-endian integer.
  bool read_u16(std::uint16_t &value) noexcept { return read_unsigned(value); }

  /// Reads an unsigned 32-bit little-endian integer.
  bool read_u32(std::uint32_t &value) noexcept { return read_unsigned(value); }

  /// Reads an unsigned 64-bit little-endian integer.
  bool read_u64(std::uint64_t &value) noexcept { return read_unsigned(value); }

  /// Reads a signed 32-bit little-endian integer.
  bool read_i32(std::int32_t &value) noexcept {
    std::uint32_t encoded{0};
    if (!read_u32(encoded)) {
      return false;
    }
    value = std::bit_cast<std::int32_t>(encoded);
    return true;
  }

  /// Reads a signed 64-bit little-endian integer.
  bool read_i64(std::int64_t &value) noexcept {
    std::uint64_t encoded{0};
    if (!read_u64(encoded)) {
      return false;
    }
    value = std::bit_cast<std::int64_t>(encoded);
    return true;
  }

  /// Reads an IEEE 754 binary64 little-endian value.
  bool read_double(double &value) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    std::uint64_t encoded{0};
    if (!read_u64(encoded)) {
      return false;
    }
    value = std::bit_cast<double>(encoded);
    return true;
  }

  /// Copies bytes from the input buffer into the supplied output buffer.
  bool read_bytes(std::span<std::byte> output) noexcept {
    if (!ensure_available(output.size())) {
      return false;
    }
    if (!output.empty()) {
      std::memcpy(output.data(), current_, output.size());
      current_ += output.size();
      remaining_ -= output.size();
    }
    return true;
  }

  /// Reports whether every attempted read has succeeded.
  [[nodiscard]] bool ok() const noexcept { return is_valid_; }

  /// Reports whether every attempted read succeeded and consumed the buffer.
  [[nodiscard]] bool complete() const noexcept {
    return is_valid_ && remaining_ == 0;
  }

  /// Returns the number of bytes consumed successfully.
  [[nodiscard]] std::size_t read() const noexcept {
    return capacity_ - remaining_;
  }

  /// Returns the number of unread bytes remaining.
  [[nodiscard]] std::size_t remaining() const noexcept { return remaining_; }

private:
  template <std::unsigned_integral Value>
  bool read_unsigned(Value &value) noexcept {
    if (!ensure_available(sizeof(value))) {
      return false;
    }
    Value encoded{0};
    std::memcpy(&encoded, current_, sizeof(encoded));
    value = detail::le2host(encoded);
    current_ += sizeof(encoded);
    remaining_ -= sizeof(encoded);
    return true;
  }

  bool ensure_available(std::size_t size) noexcept {
    if (!is_valid_ || size > remaining_) {
      is_valid_ = false;
      return false;
    }
    return true;
  }

  const std::byte *current_{nullptr};
  std::size_t capacity_{0};
  std::size_t remaining_{0};
  bool is_valid_{true};
};

} // namespace z::wire
