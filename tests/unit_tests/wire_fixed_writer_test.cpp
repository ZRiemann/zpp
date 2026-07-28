#include <array>
#include <cstddef>

#include <gtest/gtest.h>
#include <zpp/wire/fixed_writer.h>

TEST(WireFixedWriter, WritesLittleEndianValuesAndBytes) {
  std::array<std::byte, 38> buffer{};
  auto out = z::wire::fixed_writer<buffer.size()>::create(buffer);
  ASSERT_TRUE(out.has_value());
  constexpr std::array suffix{std::byte{0xaa}, std::byte{0xbb}};

  out->write_byte(std::byte{0x7f});
  out->write_u8(0x80);
  out->write_u16(0x1234);
  out->write_u32(0x12345678);
  out->write_u64(0x0102030405060708ULL);
  out->write_i32(-2);
  out->write_i64(-3);
  out->write_double(1.0);
  out->write_bytes(suffix);

  EXPECT_TRUE(out->complete());
  EXPECT_EQ(out->written(), buffer.size());
  EXPECT_EQ(out->remaining(), 0U);
  constexpr std::array expected{
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
  EXPECT_EQ(buffer, expected);
}

TEST(WireFixedWriter, RequiresExactBufferSize) {
  std::array<std::byte, 1> short_buffer{};
  std::array<std::byte, 2> exact_buffer{};
  std::array<std::byte, 3> long_buffer{};

  EXPECT_FALSE(z::wire::fixed_writer<2>::create(short_buffer).has_value());
  EXPECT_TRUE(z::wire::fixed_writer<2>::create(exact_buffer).has_value());
  EXPECT_FALSE(z::wire::fixed_writer<2>::create(long_buffer).has_value());
}

TEST(WireFixedWriter, ReportsIncompleteBlock) {
  std::array<std::byte, 2> buffer{};
  auto out = z::wire::fixed_writer<buffer.size()>::create(buffer);
  ASSERT_TRUE(out.has_value());

  out->write_u8(1);

  EXPECT_FALSE(out->complete());
  EXPECT_EQ(out->written(), 1U);
  EXPECT_EQ(out->remaining(), 1U);
}
