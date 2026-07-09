#include <array>
#include <cstddef>

#include <gtest/gtest.h>
#include <zpp/wire/writer.h>

TEST(WireWriter, WritesLittleEndianValuesAndBytes) {
  std::array<std::byte, 17> buffer{};
  z::wire::writer out{buffer};
  constexpr std::array suffix{std::byte{0xaa}, std::byte{0xbb}};

  EXPECT_TRUE(out.write_u16(0x1234));
  EXPECT_TRUE(out.write_i32(-2));
  EXPECT_TRUE(out.write_double(1.0));
  EXPECT_TRUE(out.write_byte(std::byte{0x7f}));
  EXPECT_TRUE(out.write_bytes(suffix));

  EXPECT_TRUE(out.ok());
  EXPECT_TRUE(out.complete());
  EXPECT_EQ(out.written(), buffer.size());
  EXPECT_EQ(out.remaining(), 0U);

  constexpr std::array expected{
      std::byte{0x34}, std::byte{0x12}, std::byte{0xfe}, std::byte{0xff},
      std::byte{0xff}, std::byte{0xff}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0xf0}, std::byte{0x3f}, std::byte{0x7f}, std::byte{0xaa},
      std::byte{0xbb},
  };
  EXPECT_EQ(buffer, expected);
}

TEST(WireWriter, RejectsOverflowWithoutPartialWrite) {
  std::array<std::byte, 1> buffer{std::byte{0xaa}};
  z::wire::writer out{buffer};

  EXPECT_FALSE(out.write_u16(1));
  EXPECT_FALSE(out.ok());
  EXPECT_FALSE(out.complete());
  EXPECT_EQ(out.written(), 0U);
  EXPECT_EQ(out.remaining(), buffer.size());
  EXPECT_EQ(buffer.front(), std::byte{0xaa});
  EXPECT_FALSE(out.write_byte(std::byte{0x01}));
}
