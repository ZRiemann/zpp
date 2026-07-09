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

/// Writes little-endian scalar values and raw bytes into a fixed-size buffer.
class writer {
public:
  /// Creates a writer over the supplied writable buffer.
  explicit writer(std::span<std::byte> buffer) noexcept
      : current_(buffer.data()), capacity_(buffer.size()),
        remaining_(buffer.size()) {}

  /// Writes one raw byte.
  bool write_byte(std::byte value) noexcept {
    if (!ensure_available(1)) {
      return false;
    }
    *current_++ = value;
    --remaining_;
    return true;
  }

  /// Writes an unsigned 16-bit integer in little-endian order.
  bool write_u16(std::uint16_t value) noexcept { return write_unsigned(value); }

  /// Writes an unsigned 32-bit integer in little-endian order.
  bool write_u32(std::uint32_t value) noexcept { return write_unsigned(value); }

  /// Writes an unsigned 64-bit integer in little-endian order.
  bool write_u64(std::uint64_t value) noexcept { return write_unsigned(value); }

  /// Writes a signed 32-bit integer in little-endian order.
  bool write_i32(std::int32_t value) noexcept {
    return write_u32(static_cast<std::uint32_t>(value));
  }

  /// Writes a signed 64-bit integer in little-endian order.
  bool write_i64(std::int64_t value) noexcept {
    return write_u64(static_cast<std::uint64_t>(value));
  }

  /// Writes an IEEE 754 binary64 value in little-endian order.
  bool write_double(double value) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    return write_u64(std::bit_cast<std::uint64_t>(value));
  }

  /// Copies a contiguous byte sequence into the output buffer.
  bool write_bytes(std::span<const std::byte> bytes) noexcept {
    if (!ensure_available(bytes.size())) {
      return false;
    }
    if (!bytes.empty()) {
      std::memcpy(current_, bytes.data(), bytes.size());
      current_ += bytes.size();
      remaining_ -= bytes.size();
    }
    return true;
  }

  /// Reports whether every attempted write has succeeded.
  [[nodiscard]] bool ok() const noexcept { return is_valid_; }

  /// Reports whether every attempted write succeeded and filled the buffer.
  [[nodiscard]] bool complete() const noexcept {
    return is_valid_ && remaining_ == 0;
  }

  /// Returns the number of bytes written successfully.
  [[nodiscard]] std::size_t written() const noexcept {
    return capacity_ - remaining_;
  }

  /// Returns the number of writable bytes remaining.
  [[nodiscard]] std::size_t remaining() const noexcept { return remaining_; }

private:
  template <std::unsigned_integral Value>
  bool write_unsigned(Value value) noexcept {
    if (!ensure_available(sizeof(value))) {
      return false;
    }
    value = detail::host2le(value);
    std::memcpy(current_, &value, sizeof(value));
    current_ += sizeof(value);
    remaining_ -= sizeof(value);
    return true;
  }

  bool ensure_available(std::size_t size) noexcept {
    if (!is_valid_ || size > remaining_) {
      is_valid_ = false;
      return false;
    }
    return true;
  }

  std::byte *current_{nullptr};
  std::size_t capacity_{0};
  std::size_t remaining_{0};
  bool is_valid_{true};
};

} // namespace z::wire
