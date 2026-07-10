#include <array>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <zpp/wire/msg_header.h>

TEST(WireMsgHeader, HasStablePackedLayout) {
  static_assert(sizeof(base_header) == 4);
  static_assert(sizeof(pub_header) == 5);
  EXPECT_EQ(offsetof(base_header, version), 0U);
  EXPECT_EQ(offsetof(base_header, category), 1U);
  EXPECT_EQ(offsetof(base_header, type), 2U);
  EXPECT_EQ(offsetof(pub_header, base), 0U);
  EXPECT_EQ(offsetof(pub_header, topic_count), 4U);
}

TEST(WireMsgHeader, LocatesPubTopicAndBody) {
  std::array<std::uint8_t, 8> buffer{};
  auto *header = reinterpret_cast<pub_header *>(buffer.data());
  header->base.version = 1;
  header->base.category = 2;
  header->base.type = 3;
  header->topic_count = 2;
  buffer[sizeof(pub_header)] = 10;
  buffer[sizeof(pub_header) + 1] = 20;
  buffer[sizeof(pub_header) + 2] = 30;

  ASSERT_TRUE(z::wire::is_valid_pub_message(buffer.data(), buffer.size()));
  EXPECT_EQ(z::wire::pub_header_size(header), 7U);
  EXPECT_EQ(z::wire::pub_topic_begin(header)[0], 10U);
  EXPECT_EQ(z::wire::pub_topic_begin(header)[1], 20U);
  EXPECT_EQ(*z::wire::pub_body_begin(header), 30U);
  EXPECT_EQ(z::wire::pub_body_size(header, buffer.size()), 1U);
}

TEST(WireMsgHeader, RejectsTruncatedPubTopic) {
  std::array<std::uint8_t, sizeof(pub_header)> buffer{};
  auto *header = reinterpret_cast<pub_header *>(buffer.data());
  header->topic_count = 1;

  EXPECT_FALSE(z::wire::is_valid_pub_message(buffer.data(), buffer.size()));
  EXPECT_FALSE(z::wire::is_valid_pub_message(nullptr, buffer.size()));
}
