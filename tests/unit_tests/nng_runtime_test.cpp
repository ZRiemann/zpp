#include <utility>

#include <gtest/gtest.h>
#include <zpp/nng/runtime.h>

TEST(NngRuntime, SocketOwnsAndMovesSocket) {
    nng_socket raw_socket{};
    ASSERT_EQ(nng_pair0_open(&raw_socket), 0);

    z::nng::socket socket(raw_socket);
    EXPECT_TRUE(socket.valid());

    z::nng::socket moved(std::move(socket));
    EXPECT_FALSE(socket.valid());
    EXPECT_TRUE(moved.valid());

    moved.close();
    EXPECT_FALSE(moved.valid());
}

TEST(NngRuntime, PublisherRejectsInvalidEndpoint) {
    z::nng::publisher publisher;
    const z::nng::socket_options options;

    EXPECT_NE(publisher.start({"inproc://zpp.runtime.publisher",
                               z::nng::transport::inproc,
                               z::nng::endpoint_role::listen},
                              options),
              z::ERR_OK);
    EXPECT_NE(publisher.start({"ipc:///tmp/zpp-runtime-publisher.sock",
                               z::nng::transport::ipc,
                               z::nng::endpoint_role::dial},
                              options),
              z::ERR_OK);
}

TEST(NngRuntime, NowNsReturnsPositiveEpochTimestamp) {
    EXPECT_GT(z::nng::now_ns(), 0);
}
