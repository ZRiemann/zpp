#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>
#include <zpp/wire/reader.h>

TEST(WireReader, ReadsLittleEndianValuesAndBytes) {
  constexpr std::array input{
      std::byte{0x34}, std::byte{0x12}, std::byte{0xfe}, std::byte{0xff},
      std::byte{0xff}, std::byte{0xff}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0xf0}, std::byte{0x3f}, std::byte{0x7f}, std::byte{0xaa},
      std::byte{0xbb},
  };
  z::wire::reader in{input};
  std::uint16_t u16{0};
  std::int32_t i32{0};
  double floating{0.0};
  std::byte byte{0};
  std::array<std::byte, 2> suffix{};

  EXPECT_TRUE(in.read_u16(u16));
  EXPECT_TRUE(in.read_i32(i32));
  EXPECT_TRUE(in.read_double(floating));
  EXPECT_TRUE(in.read_byte(byte));
  EXPECT_TRUE(in.read_bytes(suffix));

  EXPECT_EQ(u16, 0x1234U);
  EXPECT_EQ(i32, -2);
  EXPECT_DOUBLE_EQ(floating, 1.0);
  EXPECT_EQ(byte, std::byte{0x7f});
  EXPECT_EQ(suffix, (std::array{std::byte{0xaa}, std::byte{0xbb}}));
  EXPECT_TRUE(in.ok());
  EXPECT_TRUE(in.complete());
  EXPECT_EQ(in.read(), input.size());
  EXPECT_EQ(in.remaining(), 0U);
}

TEST(WireReader, RejectsTruncatedValueWithoutPartialRead) {
  constexpr std::array input{std::byte{0x12}};
  z::wire::reader in{input};
  std::uint16_t value{0x3456};

  EXPECT_FALSE(in.read_u16(value));
  EXPECT_EQ(value, 0x3456U);
  EXPECT_FALSE(in.ok());
  EXPECT_FALSE(in.complete());
  EXPECT_EQ(in.read(), 0U);
  EXPECT_EQ(in.remaining(), input.size());
}
