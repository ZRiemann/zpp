#include <array>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <zpp/nng/dialer.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/listener.h>
#include <zpp/nng/nng.h>
#include <zpp/nng/pipe.h>
#include <zpp/nng/socket.h>

namespace {

/// Returns an IPv4 loopback address with an ephemeral source port.
nng_sockaddr loopback_address() noexcept {
  nng_sockaddr address{};
  address.s_in.sa_family = NNG_AF_INET;
  constexpr std::array<unsigned char, 4> loopback{127, 0, 0, 1};
  std::memcpy(&address.s_in.sa_addr, loopback.data(), loopback.size());
  return address;
}

} // namespace

TEST(NngEndpoint, RejectsBindingAnInvalidDialer) {
  z::nng::nng runtime;
  z::nng::dialer dialer;
  EXPECT_EQ(dialer.bind(loopback_address()), NNG_ENOENT);
}

TEST(NngEndpoint, RejectsInvalidDialerSourceAddresses) {
  z::nng::nng runtime;
  z::nng::socket socket;
  ASSERT_EQ(socket.pair0_open(), NNG_OK);

  z::nng::dialer dialer;
  ASSERT_EQ(dialer.create(socket.get(), "tcp://127.0.0.1:1"), NNG_OK);
  EXPECT_EQ(dialer.bind(nng_sockaddr{}), NNG_EADDRINVAL);

  auto address = loopback_address();
  address.s_in.sa_port = 1;
  EXPECT_EQ(dialer.bind(address), NNG_EADDRINVAL);
  EXPECT_EQ(dialer.bind(loopback_address()), NNG_OK);
}

TEST(NngEndpoint, BindsDialerBeforeStartingLocalCommunication) {
  z::nng::nng runtime;
  z::nng::socket sender;
  z::nng::socket receiver;
  ASSERT_EQ(sender.pair0_open(), NNG_OK);
  ASSERT_EQ(receiver.pair0_open(), NNG_OK);

  z::nng::socket_options options;
  options.send_timeout_ms = 1000;
  options.recv_timeout_ms = 1000;
  ASSERT_EQ(sender.set_options(options), NNG_OK);
  ASSERT_EQ(receiver.set_options(options), NNG_OK);

  z::nng::listener listener;
  ASSERT_EQ(listener.create(receiver.get(), "tcp://127.0.0.1:0"), NNG_OK);
  ASSERT_EQ(listener.start(), NNG_OK);
  int port{0};
  ASSERT_EQ(listener.get_int(NNG_OPT_BOUND_PORT, &port), NNG_OK);
  ASSERT_GT(port, 0);

  z::nng::dialer dialer;
  const auto target_url = "tcp://127.0.0.1:" + std::to_string(port);
  ASSERT_EQ(dialer.create(sender.get(), target_url), NNG_OK);
  const auto local_address = loopback_address();
  ASSERT_EQ(dialer.bind(local_address), NNG_OK);
  ASSERT_EQ(dialer.start(0), NNG_OK);

  constexpr std::string_view payload{"bound-dialer"};
  z::nng::message outgoing{std::size_t{0}};
  ASSERT_TRUE(outgoing.valid());
  ASSERT_EQ(outgoing.append(payload.data(), payload.size()), NNG_OK);
  ASSERT_EQ(sender.send(outgoing, 0), NNG_OK);
  EXPECT_FALSE(outgoing.valid());

  z::nng::message incoming;
  ASSERT_EQ(receiver.recv(incoming), NNG_OK);
  EXPECT_EQ(std::string_view(static_cast<const char *>(incoming.body()),
                             incoming.length()),
            payload);

  const z::nng::pipe connection{incoming.pipe()};
  nng_sockaddr peer_address{};
  ASSERT_EQ(connection.peer_addr(peer_address), NNG_OK);
  EXPECT_EQ(peer_address.s_in.sa_family, local_address.s_in.sa_family);
  EXPECT_EQ(peer_address.s_in.sa_addr, local_address.s_in.sa_addr);
}

TEST(NngEndpoint, ParsesTransportFromUrls) {
  EXPECT_EQ(z::nng::parse_transport("tcp://127.0.0.1:58200"),
            z::nng::transport::tcp);
  EXPECT_EQ(z::nng::parse_transport("ipc:///tmp/a.sock"),
            z::nng::transport::ipc);
  EXPECT_EQ(z::nng::parse_transport("inproc://worker"),
            z::nng::transport::inproc);
  EXPECT_EQ(z::nng::parse_transport("udp://127.0.0.1:58200"),
            z::nng::transport::unknown);
}

TEST(NngEndpoint, ValidatesTcpPortsAndFormatsUrls) {
  EXPECT_TRUE(z::nng::tcp_port_valid(1));
  EXPECT_TRUE(z::nng::tcp_port_valid(65535));
  EXPECT_FALSE(z::nng::tcp_port_valid(0));
  EXPECT_FALSE(z::nng::tcp_port_valid(65536));
  EXPECT_EQ(z::nng::make_tcp_url("203.0.113.10", 58201),
            "tcp://203.0.113.10:58201");
}

TEST(NngEndpoint, ReplacesUrlTokens) {
  EXPECT_EQ(
      z::nng::replace_url_token("ipc:///tmp/zeta.out.sock", ".out.", ".in."),
      "ipc:///tmp/zeta.in.sock");
  EXPECT_EQ(
      z::nng::replace_url_token("ipc:///tmp/zeta.out.sock", ".out.", ".rep."),
      "ipc:///tmp/zeta.rep.sock");
  EXPECT_EQ(z::nng::replace_url_token("ipc:///tmp/zeta.sock", ".out.", ".in."),
            "ipc:///tmp/zeta.sock");
}

TEST(NngEndpoint, ExtractsIpcPathAndRemovesStaleSockets) {
  EXPECT_EQ(z::nng::ipc_path("ipc:///tmp/a.sock"), "/tmp/a.sock");
  EXPECT_TRUE(z::nng::ipc_path("tcp://127.0.0.1:58200").empty());

  std::string error;
  EXPECT_EQ(z::nng::remove_stale_ipc_socket("tcp://127.0.0.1:58200", &error),
            z::ERR_OK);
  EXPECT_TRUE(error.empty());

  const std::string path = "/tmp/zpp_nng_endpoint_test.sock";
  {
    std::ofstream file(path);
    file << "stale";
  }
  ASSERT_TRUE(std::ifstream(path).good());
  EXPECT_EQ(z::nng::remove_stale_ipc_socket("ipc://" + path, &error),
            z::ERR_OK);
  EXPECT_FALSE(std::ifstream(path).good());
}

TEST(NngEndpoint, ValidatesEndpointMetadata) {
  const z::nng::endpoint valid{
      "tcp://127.0.0.1:58200",
      z::nng::transport::tcp,
      z::nng::endpoint_role::listen,
  };
  EXPECT_EQ(z::nng::validate_endpoint(valid), z::ERR_OK);
  EXPECT_NE(z::nng::validate_endpoint(
                {"", z::nng::transport::tcp, z::nng::endpoint_role::listen}),
            z::ERR_OK);
  EXPECT_NE(z::nng::validate_endpoint({"udp://127.0.0.1:58200",
                                       z::nng::transport::unknown,
                                       z::nng::endpoint_role::dial}),
            z::ERR_OK);
  EXPECT_NE(z::nng::validate_endpoint({"ipc:///tmp/metadata.sock",
                                       z::nng::transport::tcp,
                                       z::nng::endpoint_role::listen}),
            z::ERR_OK);
  EXPECT_NE(z::nng::validate_endpoint({"inproc://invalid-role",
                                       z::nng::transport::inproc,
                                       static_cast<z::nng::endpoint_role>(-1)}),
            z::ERR_OK);
}

TEST(NngEndpoint, ValidatesNonEmptyEndpointLists) {
  const std::vector<z::nng::endpoint> valid{
      {"inproc://endpoint-list", z::nng::transport::inproc,
       z::nng::endpoint_role::dial},
  };
  EXPECT_EQ(z::nng::validate_endpoints(valid), z::ERR_OK);
  EXPECT_NE(z::nng::validate_endpoints({}), z::ERR_OK);
  EXPECT_NE(z::nng::validate_endpoints(
                {{"tcp://127.0.0.1:58200", z::nng::transport::ipc,
                  z::nng::endpoint_role::listen}}),
            z::ERR_OK);
}

#ifndef _WIN32
TEST(NngEndpoint, AppliesOwnerOnlyIpcListenerPermissions) {
  z::nng::nng runtime;
  z::nng::socket socket;
  ASSERT_EQ(socket.rep0_open(), NNG_OK);

  z::nng::listener listener;
  ASSERT_EQ(listener.create(socket.get(),
                            "ipc:///tmp/zpp_nng_listener_options.sock"),
            NNG_OK);

  z::nng::listener_options options;
  options.ipc_permissions = 0600;
  ASSERT_EQ(listener.set_options(&options, nullptr), NNG_OK);

  options.ipc_permissions = 01000;
  EXPECT_EQ(listener.set_options(&options, nullptr), NNG_EINVAL);
}
#endif
