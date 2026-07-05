#include <utility>

#include <string>

#include <gtest/gtest.h>
#include <zpp/nng/url.h>

TEST(NngUrl, ParsesFieldsAndFormats) {
  z::nng::url url;

  ASSERT_EQ(url.parse("tcp://127.0.0.1:58200/path?key=value#frag"), 0);
  ASSERT_TRUE(url.valid());
  EXPECT_STREQ(url.scheme(), "tcp");
  EXPECT_STREQ(url.hostname(), "127.0.0.1");
  EXPECT_EQ(url.port(), 58200U);
  EXPECT_STREQ(url.path(), "/path");
  EXPECT_STREQ(url.query(), "key=value");
  EXPECT_STREQ(url.fragment(), "frag");
  EXPECT_EQ(url.str(), "tcp://127.0.0.1:58200/path?key=value#frag");
}

TEST(NngUrl, MovesOwnership) {
  z::nng::url source;
  ASSERT_EQ(source.parse("ipc:///tmp/zpp_nng_url_test.sock"), 0);

  z::nng::url moved{std::move(source)};
  EXPECT_FALSE(source.valid());
  ASSERT_TRUE(moved.valid());
  EXPECT_STREQ(moved.scheme(), "ipc");
  EXPECT_STREQ(moved.path(), "/tmp/zpp_nng_url_test.sock");
}

TEST(NngUrl, ClonesUrl) {
  z::nng::url source;
  ASSERT_EQ(source.parse("inproc://zpp.url.test"), 0);

  z::nng::url cloned;
  ASSERT_EQ(source.clone(cloned), 0);
  ASSERT_TRUE(cloned.valid());
  EXPECT_STREQ(cloned.scheme(), "inproc");
  EXPECT_STREQ(cloned.path(), "zpp.url.test");
  EXPECT_EQ(cloned.str(), source.str());
}

TEST(NngUrl, ResolvesZeroPort) {
  z::nng::url url;

  ASSERT_EQ(url.parse("tcp://127.0.0.1:0"), 0);
  EXPECT_EQ(url.port(), 0U);
  url.resolve_port(58201);
  EXPECT_EQ(url.port(), 58201U);
  EXPECT_EQ(url.str(), "tcp://127.0.0.1:58201");
}

TEST(NngUrl, ReportsInvalidParse) {
  z::nng::url url;

  EXPECT_NE(url.parse("www.google.com"), 0);
  EXPECT_FALSE(url.valid());
}
