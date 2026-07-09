#include <array>
#include <cstddef>
#include <limits>

#include <gtest/gtest.h>
#include <zpp/wire/size_counter.h>
#include <zpp/wire/writer.h>

namespace {

template <typename Sink> bool write_sample(Sink &out) {
  constexpr std::array suffix{std::byte{0xaa}, std::byte{0xbb}};
  return out.write_u16(0x1234) && out.write_i32(-2) && out.write_double(1.0) &&
         out.write_byte(std::byte{0x7f}) && out.write_bytes(suffix);
}

} // namespace

TEST(WireSizeCounter, MatchesWriterOperationSequence) {
  z::wire::size_counter counter;
  ASSERT_TRUE(write_sample(counter));
  ASSERT_TRUE(counter.ok());

  std::array<std::byte, 17> buffer{};
  ASSERT_EQ(counter.size(), buffer.size());
  EXPECT_EQ(counter.written(), buffer.size());

  z::wire::writer out{buffer};
  EXPECT_TRUE(write_sample(out));
  EXPECT_TRUE(out.complete());
  EXPECT_EQ(out.written(), counter.size());
}

TEST(WireSizeCounter, DetectsSizeOverflow) {
  z::wire::size_counter counter{std::numeric_limits<std::size_t>::max()};

  EXPECT_FALSE(counter.write_byte(std::byte{0}));
  EXPECT_FALSE(counter.ok());
  EXPECT_EQ(counter.size(), std::numeric_limits<std::size_t>::max());
}
