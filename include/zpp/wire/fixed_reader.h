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

/// Reads a compile-time-sized wire block after one exact-size validation.
///
/// Every read must fit in the validated block. Debug builds assert this
/// contract; release builds intentionally omit per-operation bounds checks.
template <std::size_t Size> class fixed_reader {
public:
  /// Creates a reader only when the supplied buffer is exactly Size bytes.
  [[nodiscard]] static std::optional<fixed_reader>
  create(std::span<const std::byte> buffer) noexcept {
    if (buffer.size() != Size) {
      return std::nullopt;
    }
    return fixed_reader{buffer.data()};
  }

  /// Reads one raw byte.
  void read_byte(std::byte &value) noexcept {
    assert_available(1);
    value = buffer_[position_++];
  }

  /// Reads an unsigned 8-bit integer.
  void read_u8(std::uint8_t &value) noexcept {
    std::byte encoded{0};
    read_byte(encoded);
    value = std::to_integer<std::uint8_t>(encoded);
  }

  /// Reads an unsigned 16-bit little-endian integer.
  void read_u16(std::uint16_t &value) noexcept { read_unsigned(value); }

  /// Reads an unsigned 32-bit little-endian integer.
  void read_u32(std::uint32_t &value) noexcept { read_unsigned(value); }

  /// Reads an unsigned 64-bit little-endian integer.
  void read_u64(std::uint64_t &value) noexcept { read_unsigned(value); }

  /// Reads a signed 32-bit little-endian integer.
  void read_i32(std::int32_t &value) noexcept {
    std::uint32_t encoded{0};
    read_u32(encoded);
    value = std::bit_cast<std::int32_t>(encoded);
  }

  /// Reads a signed 64-bit little-endian integer.
  void read_i64(std::int64_t &value) noexcept {
    std::uint64_t encoded{0};
    read_u64(encoded);
    value = std::bit_cast<std::int64_t>(encoded);
  }

  /// Reads an IEEE 754 binary64 little-endian value.
  void read_double(double &value) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    std::uint64_t encoded{0};
    read_u64(encoded);
    value = std::bit_cast<double>(encoded);
  }

  /// Copies bytes from the input block into the supplied output buffer.
  void read_bytes(std::span<std::byte> output) noexcept {
    assert_available(output.size());
    if (!output.empty()) {
      std::memcpy(output.data(), buffer_ + position_, output.size());
      position_ += output.size();
    }
  }

  /// Reports whether the entire fixed block has been consumed.
  [[nodiscard]] bool complete() const noexcept { return position_ == Size; }

  /// Returns the number of bytes consumed.
  [[nodiscard]] std::size_t read() const noexcept { return position_; }

  /// Returns the number of bytes not yet consumed.
  [[nodiscard]] std::size_t remaining() const noexcept {
    return Size - position_;
  }

private:
  explicit fixed_reader(const std::byte *buffer) noexcept : buffer_(buffer) {}

  void assert_available([[maybe_unused]] std::size_t size) const noexcept {
    assert(size <= remaining());
  }

  template <std::unsigned_integral Value>
  void read_unsigned(Value &value) noexcept {
    assert_available(sizeof(value));
    Value encoded{0};
    std::memcpy(&encoded, buffer_ + position_, sizeof(encoded));
    value = detail::le2host(encoded);
    position_ += sizeof(encoded);
  }

  const std::byte *buffer_{nullptr};
  std::size_t position_{0};
};

} // namespace z::wire
