#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace z::wire {

static_assert(CHAR_BIT == 8, "zpp wire requires 8-bit bytes");

/// Counts the bytes that the matching writer operations would produce.
class size_counter {
public:
  /// Creates a counter with an optional number of bytes already accounted for.
  explicit size_counter(std::size_t initial_size = 0) noexcept
      : size_(initial_size) {}

  /// Accounts for one raw byte.
  bool write_byte(std::byte) noexcept { return add(1); }

  /// Accounts for an encoded unsigned 16-bit integer.
  bool write_u16(std::uint16_t) noexcept { return add(sizeof(std::uint16_t)); }

  /// Accounts for an encoded unsigned 32-bit integer.
  bool write_u32(std::uint32_t) noexcept { return add(sizeof(std::uint32_t)); }

  /// Accounts for an encoded unsigned 64-bit integer.
  bool write_u64(std::uint64_t) noexcept { return add(sizeof(std::uint64_t)); }

  /// Accounts for an encoded signed 32-bit integer.
  bool write_i32(std::int32_t) noexcept { return add(sizeof(std::int32_t)); }

  /// Accounts for an encoded signed 64-bit integer.
  bool write_i64(std::int64_t) noexcept { return add(sizeof(std::int64_t)); }

  /// Accounts for an encoded IEEE 754 binary64 value.
  bool write_double(double) noexcept {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    static_assert(std::numeric_limits<double>::is_iec559);
    return add(sizeof(double));
  }

  /// Accounts for a contiguous byte sequence.
  bool write_bytes(std::span<const std::byte> bytes) noexcept {
    return add(bytes.size());
  }

  /// Reports whether every attempted size addition has succeeded.
  [[nodiscard]] bool ok() const noexcept { return is_valid_; }

  /// Returns the total encoded size accumulated successfully.
  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  /// Returns the total encoded size accumulated successfully.
  [[nodiscard]] std::size_t written() const noexcept { return size_; }

private:
  bool add(std::size_t size) noexcept {
    if (!is_valid_ || size > std::numeric_limits<std::size_t>::max() - size_) {
      is_valid_ = false;
      return false;
    }
    size_ += size;
    return true;
  }

  std::size_t size_{0};
  bool is_valid_{true};
};

} // namespace z::wire
