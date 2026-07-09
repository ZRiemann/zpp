#pragma once

#include <bit>
#include <climits>
#include <concepts>
#include <cstddef>

namespace z::wire::detail {

static_assert(CHAR_BIT == 8, "zpp wire requires 8-bit bytes");

template <typename Value>
concept unsigned_integer =
    std::unsigned_integral<Value> && !std::same_as<Value, bool>;

template <unsigned_integer Value>
[[nodiscard]] constexpr Value byteswap(Value value) noexcept {
  Value swapped{0};

  for (std::size_t i = 0; i < sizeof(Value); ++i) {
    swapped = static_cast<Value>((swapped << 8U) | (value & Value{0xff}));
    value >>= 8U;
  }

  return swapped;
}

template <unsigned_integer Value>
[[nodiscard]] constexpr Value host2le(Value value) noexcept {
  static_assert(std::endian::native == std::endian::little ||
                std::endian::native == std::endian::big,
                "zpp wire requires little-endian or big-endian host");

  if constexpr (std::endian::native == std::endian::little) {
    return value;
  } else {
    return byteswap(value);
  }
}

template <unsigned_integer Value>
[[nodiscard]] constexpr Value le2host(Value value) noexcept {
  return host2le(value);
}

} // namespace z::wire::detail