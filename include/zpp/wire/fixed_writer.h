#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

#include <zpp/wire/detail/endian.h>

namespace z::wire {

/// Writes a compile-time-sized wire block after one exact-size validation.
///
/// Every write must fit in the validated block. Debug builds assert this
/// contract; release builds intentionally omit per-operation bounds checks.
template <std::size_t Size> class fixed_writer {
public:
  /// Creates a writer only when the supplied buffer is exactly Size bytes.
  [[nodiscard]] static std::optional<fixed_writer>
  create(std::span<std::byte> buffer) noexcept {
    if (buffer.size() != Size) {
      return std::nullopt;
    }
    return fixed_writer{buffer.data()};
  }

  /// Writes one raw byte.
  void write_byte(std::byte value) noexcept {
    assert_available(1);
    buffer_[position_++] = value;
  }

  /// Writes an unsigned 8-bit integer.
  void write_u8(std::uint8_t value) noexcept {
    write_byte(static_cast<std::byte>(value));
  }

  /// Writes an unsigned 16-bit integer in little-endian order.
  void write_u16(std::uint16_t value) noexcept { write_unsigned(value); }

  /// Writes an unsigned 32-bit integer in little-endian order.
  void write_u32(std::uint32_t value) noexcept { write_unsigned(value); }

  /// Writes an unsigned 64-bit integer in little-endian order.
  void write_u64(std::uint64_t value) noexcept { write_unsigned(value); }

  /// Writes a signed 32-bit integer in little-endian order.
  void write_i32(std::int32_t value) noexcept {
    write_u32(static_cast<std::uint32_t>(value));
  }

  /// Writes a signed 64-bit integer in little-endian order.
  void write_i64(std::int64_t value) noexcept {
    write_u64(static_cast<std::uint64_t>(value));
  }

  /// Writes an IEEE 754 binary64 value in little-endian order.
  void write_double(double value) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    write_u64(std::bit_cast<std::uint64_t>(value));
  }

  /// Copies a contiguous byte sequence into the output block.
  void write_bytes(std::span<const std::byte> bytes) noexcept {
    assert_available(bytes.size());
    if (!bytes.empty()) {
      std::memcpy(buffer_ + position_, bytes.data(), bytes.size());
      position_ += bytes.size();
    }
  }

  /// Reports whether the entire fixed block has been written.
  [[nodiscard]] bool complete() const noexcept { return position_ == Size; }

  /// Returns the number of bytes written.
  [[nodiscard]] std::size_t written() const noexcept { return position_; }

  /// Returns the number of bytes not yet written.
  [[nodiscard]] std::size_t remaining() const noexcept {
    return Size - position_;
  }

private:
  explicit fixed_writer(std::byte *buffer) noexcept : buffer_(buffer) {}

  void assert_available([[maybe_unused]] std::size_t size) const noexcept {
    assert(size <= remaining());
  }

  template <std::unsigned_integral Value>
  void write_unsigned(Value value) noexcept {
    assert_available(sizeof(value));
    value = detail::host2le(value);
    std::memcpy(buffer_ + position_, &value, sizeof(value));
    position_ += sizeof(value);
  }

  std::byte *buffer_{nullptr};
  std::size_t position_{0};
};

} // namespace z::wire
