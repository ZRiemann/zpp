#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>
#include <zpp/wire/fixed_reader.h>

TEST(WireFixedReader, ReadsLittleEndianValuesAndBytes) {
  constexpr std::array input{
      std::byte{0x7f}, std::byte{0x80}, std::byte{0x34}, std::byte{0x12},
      std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12},
      std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
      std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
      std::byte{0xfe}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
      std::byte{0xfd}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
      std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0xf0}, std::byte{0x3f},
      std::byte{0xaa}, std::byte{0xbb},
  };
  auto in = z::wire::fixed_reader<input.size()>::create(input);
  ASSERT_TRUE(in.has_value());
  std::byte byte{0};
  std::uint8_t u8{0};
  std::uint16_t u16{0};
  std::uint32_t u32{0};
  std::uint64_t u64{0};
  std::int32_t i32{0};
  std::int64_t i64{0};
  double floating{0.0};
  std::array<std::byte, 2> suffix{};

  in->read_byte(byte);
  in->read_u8(u8);
  in->read_u16(u16);
  in->read_u32(u32);
  in->read_u64(u64);
  in->read_i32(i32);
  in->read_i64(i64);
  in->read_double(floating);
  in->read_bytes(suffix);

  EXPECT_EQ(byte, std::byte{0x7f});
  EXPECT_EQ(u8, 0x80U);
  EXPECT_EQ(u16, 0x1234U);
  EXPECT_EQ(u32, 0x12345678U);
  EXPECT_EQ(u64, 0x0102030405060708ULL);
  EXPECT_EQ(i32, -2);
  EXPECT_EQ(i64, -3);
  EXPECT_DOUBLE_EQ(floating, 1.0);
  EXPECT_EQ(suffix, (std::array{std::byte{0xaa}, std::byte{0xbb}}));
  EXPECT_TRUE(in->complete());
  EXPECT_EQ(in->read(), input.size());
  EXPECT_EQ(in->remaining(), 0U);
}

TEST(WireFixedReader, RequiresExactBufferSize) {
  std::array<std::byte, 1> short_buffer{};
  std::array<std::byte, 2> exact_buffer{};
  std::array<std::byte, 3> long_buffer{};

  EXPECT_FALSE(z::wire::fixed_reader<2>::create(short_buffer).has_value());
  EXPECT_TRUE(z::wire::fixed_reader<2>::create(exact_buffer).has_value());
  EXPECT_FALSE(z::wire::fixed_reader<2>::create(long_buffer).has_value());
}

TEST(WireFixedReader, ReportsIncompleteBlock) {
  constexpr std::array input{std::byte{1}, std::byte{2}};
  auto in = z::wire::fixed_reader<input.size()>::create(input);
  ASSERT_TRUE(in.has_value());
  std::uint8_t value{0};

  in->read_u8(value);

  EXPECT_EQ(value, 1U);
  EXPECT_FALSE(in->complete());
  EXPECT_EQ(in->read(), 1U);
  EXPECT_EQ(in->remaining(), 1U);
}
