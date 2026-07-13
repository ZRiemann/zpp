#include <utility>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <zpp/nng/dialer.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/listener.h>
#include <zpp/nng/log.h>
#include <zpp/nng/nng.h>
#include <zpp/nng/protocols/pub_sub_device.h>
#include <zpp/nng/protocols/publisher.h>
#include <zpp/nng/protocols/subscriber.h>
#include <zpp/nng/socket.h>
#include <zpp/nng/statistics.h>
#include <zpp/spdlog.h>

namespace {

class pipe_observer_socket : public z::nng::socket {
public:
  void on_pipe_add_pre(z::nng::pipe &pipe) override {
    z::nng::socket::on_pipe_add_pre(pipe);
    ++add_pre_count;
  }

  void on_pipe_add_post(z::nng::pipe &pipe) override {
    z::nng::socket::on_pipe_add_post(pipe);
    ++add_post_count;
  }

  void on_pipe_remove_post(z::nng::pipe &pipe) override {
    z::nng::socket::on_pipe_remove_post(pipe);
    ++remove_post_count;
  }

  std::atomic<int> add_pre_count{0};
  std::atomic<int> add_post_count{0};
  std::atomic<int> remove_post_count{0};
};

/// Managed subscriber used by PUB/SUB runtime integration tests.
class recording_subscriber : public z::nng::subscriber {
public:
  /// Waits until at least count messages have been received.
  bool wait_for(std::size_t count) {
    std::unique_lock lock{mutex_};
    return completed_.wait_for(lock, std::chrono::seconds{2}, [this, count] {
      return messages_.size() >= count;
    });
  }

  /// Returns true when one received body exactly matches payload.
  bool contains(std::string_view payload) const {
    std::lock_guard lock{mutex_};
    return std::find(messages_.begin(), messages_.end(), payload) !=
           messages_.end();
  }

  /// Returns the number of successfully received messages.
  std::size_t count() const {
    std::lock_guard lock{mutex_};
    return messages_.size();
  }

protected:
  /// Copies successful message bodies for assertions.
  void on_receive(z::nng::message &msg, nng_err result) noexcept override {
    if (result != NNG_OK || !msg.valid()) {
      return;
    }
    {
      std::lock_guard lock{mutex_};
      messages_.emplace_back(static_cast<const char *>(msg.body()),
                             msg.length());
    }
    completed_.notify_all();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable completed_;
  std::vector<std::string> messages_;
};

bool wait_for_count(const std::atomic<int> &count, int expected) {
  for (auto attempts = 0; attempts < 100; ++attempts) {
    if (count.load() >= expected) {
      return true;
    }
    nng_msleep(10);
  }
  return false;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

std::vector<std::string> read_lines(const std::filesystem::path &path) {
  std::ifstream in(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

std::size_t count_substring(std::string_view text, std::string_view needle) {
  std::size_t count = 0;
  std::size_t offset = 0;
  while (true) {
    offset = text.find(needle, offset);
    if (offset == std::string_view::npos) {
      return count;
    }
    ++count;
    offset += needle.size();
  }
}

z::log::config file_log_config(const std::filesystem::path &path) {
  z::log::config cfg;
  cfg.console = false;
  cfg.rotating_file = true;
  cfg.rotate_on_open = true;
  cfg.file_name = path.string();
  cfg.max_file_size_mb = 1;
  cfg.max_files = 1;
  cfg.flush_interval = std::chrono::seconds(0);
  return cfg;
}

void require_stats_snapshot(z::nng::stats_snapshot &snapshot) {
  const auto ret = snapshot.refresh();
  if (ret == NNG_ENOTSUP) {
    GTEST_SKIP() << "NNG statistics support is disabled in this build";
  }
  ASSERT_EQ(ret, 0) << nng_strerror(static_cast<nng_err>(ret));
}

std::string unique_inproc_url(std::string_view suffix, const void *owner) {
  std::ostringstream out;
  out << "inproc://zpp.runtime." << suffix << "."
      << reinterpret_cast<std::uintptr_t>(owner);
  return out.str();
}

bool send_nonblocking(nng_socket socket, std::string_view payload) {
  for (auto attempts = 0; attempts < 100; ++attempts) {
    const auto ret =
        nng_send(socket, payload.data(), payload.size(), NNG_FLAG_NONBLOCK);
    if (ret == 0) {
      return true;
    }
    if (ret != NNG_EAGAIN) {
      return false;
    }
    nng_msleep(10);
  }
  return false;
}

bool recv_nonblocking(nng_socket socket, std::string_view expected) {
  for (auto attempts = 0; attempts < 100; ++attempts) {
    std::array<char, 64> data{};
    std::size_t size = data.size();
    const auto ret = nng_recv(socket, data.data(), &size, NNG_FLAG_NONBLOCK);
    if (ret == 0) {
      const auto matches = expected.size() == size &&
                           std::string_view{data.data(), size} == expected;
      return matches;
    }
    if (ret != NNG_EAGAIN) {
      return false;
    }
    nng_msleep(10);
  }
  return false;
}

} // namespace

TEST(NngRuntime, MessageReleaseLeavesWrapperEmpty) {
  z::nng::message message{(size_t)0};

  nng_msg *released = message.release();

  ASSERT_NE(released, nullptr);
  EXPECT_EQ(message.msg_, nullptr);
  nng_msg_free(released);
}

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

TEST(NngRuntime, SocketCanOpenRawPair0) {
  z::nng::socket socket;

  ASSERT_EQ(socket.pair0_open(true), 0);
  EXPECT_TRUE(socket.valid());

  socket.close();
  EXPECT_FALSE(socket.valid());
}

TEST(NngRuntime, SocketSetsAndGetsOptionGroups) {
  z::nng::socket socket;
  ASSERT_EQ(socket.pair0_open(), NNG_OK);

  const z::nng::socket_options expected{250, 500, 32, 64};
  ASSERT_EQ(socket.set_options(expected), NNG_OK);

  z::nng::socket_options actual;
  ASSERT_EQ(socket.get_options(actual), NNG_OK);
  EXPECT_EQ(actual.send_timeout_ms, expected.send_timeout_ms);
  EXPECT_EQ(actual.recv_timeout_ms, expected.recv_timeout_ms);
  EXPECT_EQ(actual.send_buffer, expected.send_buffer);
  EXPECT_EQ(actual.recv_buffer, expected.recv_buffer);
}

TEST(NngRuntime, SocketGetOptionsDoesNotPartiallyUpdateOnFailure) {
  z::nng::socket socket;
  const z::nng::socket_options expected;
  z::nng::socket_options actual{11, 22, 33, 44};

  EXPECT_NE(socket.set_options(expected), NNG_OK);
  EXPECT_NE(socket.get_options(actual), NNG_OK);
  EXPECT_EQ(actual.send_timeout_ms, 11);
  EXPECT_EQ(actual.recv_timeout_ms, 22);
  EXPECT_EQ(actual.send_buffer, 33);
  EXPECT_EQ(actual.recv_buffer, 44);
}

TEST(NngRuntime, DialerOwnsAndMovesHandle) {
  z::nng::nng runtime;
  z::nng::socket socket;
  const auto url = unique_inproc_url("dialer-move", &socket);

  ASSERT_EQ(socket.pair0_open(), 0);

  z::nng::dialer dialer;
  ASSERT_EQ(dialer.create(socket.get(), url), 0);
  EXPECT_TRUE(dialer.valid());

  z::nng::dialer moved(std::move(dialer));
  EXPECT_FALSE(dialer.valid());
  EXPECT_TRUE(moved.valid());

  moved.close();
  EXPECT_FALSE(moved.valid());
}

TEST(NngRuntime, ListenerOwnsAndMovesHandle) {
  z::nng::nng runtime;
  z::nng::socket socket;
  const auto url = unique_inproc_url("listener-move", &socket);

  ASSERT_EQ(socket.pair0_open(), 0);

  z::nng::listener listener;
  ASSERT_EQ(listener.create(socket.get(), url), 0);
  EXPECT_TRUE(listener.valid());

  z::nng::listener moved(std::move(listener));
  EXPECT_FALSE(listener.valid());
  EXPECT_TRUE(moved.valid());

  moved.close();
  EXPECT_FALSE(moved.valid());
}

TEST(NngRuntime, DialerAndListenerCanStartWithOptions) {
  z::nng::nng runtime;
  z::nng::socket sender;
  z::nng::socket receiver;
  const auto url = unique_inproc_url("endpoint-wrappers", &sender);

  ASSERT_EQ(sender.pair0_open(), 0);
  ASSERT_EQ(receiver.pair0_open(), 0);

  z::nng::listener listener;
  z::nng::dialer dialer;
  ASSERT_EQ(listener.create(sender.get(), std::string_view{url}), 0);
  ASSERT_EQ(dialer.create(receiver.get(), std::string_view{url}), 0);

  EXPECT_EQ(dialer.set_ms(NNG_OPT_RECONNMINT, 10), 0);
  EXPECT_EQ(dialer.set_ms(NNG_OPT_RECONNMAXT, 100), 0);

  const nng_url *listener_url{nullptr};
  const nng_url *dialer_url{nullptr};
  EXPECT_EQ(listener.get_url(&listener_url), 0);
  EXPECT_EQ(dialer.get_url(&dialer_url), 0);
  EXPECT_NE(listener_url, nullptr);
  EXPECT_NE(dialer_url, nullptr);

  ASSERT_EQ(listener.start(), 0);
  ASSERT_EQ(dialer.start(NNG_FLAG_NONBLOCK), 0);

  constexpr std::string_view payload{"endpoint-wrapper-payload"};
  ASSERT_TRUE(send_nonblocking(sender.get(), payload));
  ASSERT_TRUE(recv_nonblocking(receiver.get(), payload));
}

TEST(NngRuntime, ListenerSetsAndGetsOptionalOptionGroups) {
  z::nng::nng runtime;
  z::nng::socket socket;
  ASSERT_EQ(socket.pair0_open(), NNG_OK);

  z::nng::listener listener;
  ASSERT_EQ(listener.create(socket.get(), "tcp://127.0.0.1:0"), NNG_OK);
  const z::nng::listener_options expected_listener{2097152};
  const z::nng::transport_options expected_transport{false, true};
  ASSERT_EQ(listener.set_options(&expected_listener, &expected_transport),
            NNG_OK);

  z::nng::listener_options actual_listener;
  z::nng::transport_options actual_transport;
  ASSERT_EQ(listener.get_options(&actual_listener, &actual_transport), NNG_OK);
  EXPECT_EQ(actual_listener.max_recv_size_bytes,
            expected_listener.max_recv_size_bytes);
  EXPECT_EQ(actual_transport.tcp_no_delay, expected_transport.tcp_no_delay);
  EXPECT_EQ(actual_transport.tcp_keepalive, expected_transport.tcp_keepalive);
  EXPECT_EQ(listener.set_options(nullptr, nullptr), NNG_OK);
  EXPECT_EQ(listener.get_options(nullptr, nullptr), NNG_OK);
}

TEST(NngRuntime, DialerSetsAndGetsOptionalOptionGroups) {
  z::nng::nng runtime;
  z::nng::socket socket;
  ASSERT_EQ(socket.pair0_open(), NNG_OK);

  z::nng::dialer dialer;
  ASSERT_EQ(dialer.create(socket.get(), "tcp://127.0.0.1:58299"), NNG_OK);
  const z::nng::dialer_options expected_dialer{3145728, 25, 250};
  const z::nng::transport_options expected_transport{false, true};
  ASSERT_EQ(dialer.set_options(&expected_dialer, &expected_transport), NNG_OK);

  z::nng::dialer_options actual_dialer;
  z::nng::transport_options actual_transport;
  ASSERT_EQ(dialer.get_options(&actual_dialer, &actual_transport), NNG_OK);
  EXPECT_EQ(actual_dialer.max_recv_size_bytes,
            expected_dialer.max_recv_size_bytes);
  EXPECT_EQ(actual_dialer.reconnect_min_ms, expected_dialer.reconnect_min_ms);
  EXPECT_EQ(actual_dialer.reconnect_max_ms, expected_dialer.reconnect_max_ms);
  EXPECT_EQ(actual_transport.tcp_no_delay, expected_transport.tcp_no_delay);
  EXPECT_EQ(actual_transport.tcp_keepalive, expected_transport.tcp_keepalive);
  EXPECT_EQ(dialer.set_options(nullptr, nullptr), NNG_OK);
  EXPECT_EQ(dialer.get_options(nullptr, nullptr), NNG_OK);
}

TEST(NngRuntime, EndpointGetOptionsDoesNotPartiallyUpdateOnFailure) {
  z::nng::listener listener;
  z::nng::listener_options listener_options{111};
  z::nng::transport_options listener_transport{false, false};
  EXPECT_NE(listener.set_options(&listener_options, &listener_transport),
            NNG_OK);
  EXPECT_NE(listener.get_options(&listener_options, &listener_transport),
            NNG_OK);
  EXPECT_EQ(listener_options.max_recv_size_bytes, 111U);
  EXPECT_FALSE(listener_transport.tcp_no_delay);
  EXPECT_FALSE(listener_transport.tcp_keepalive);

  z::nng::dialer dialer;
  z::nng::dialer_options dialer_options{222, 33, 44};
  z::nng::transport_options dialer_transport{false, false};
  EXPECT_NE(dialer.set_options(&dialer_options, &dialer_transport), NNG_OK);
  EXPECT_NE(dialer.get_options(&dialer_options, &dialer_transport), NNG_OK);
  EXPECT_EQ(dialer_options.max_recv_size_bytes, 222U);
  EXPECT_EQ(dialer_options.reconnect_min_ms, 33);
  EXPECT_EQ(dialer_options.reconnect_max_ms, 44);
  EXPECT_FALSE(dialer_transport.tcp_no_delay);
  EXPECT_FALSE(dialer_transport.tcp_keepalive);
}

TEST(NngRuntime, DialerRejectsInvalidSocketOrUrl) {
  z::nng::dialer dialer;

  EXPECT_NE(dialer.create(NNG_SOCKET_INITIALIZER, ""), 0);
  EXPECT_FALSE(dialer.valid());

  EXPECT_NE(dialer.create(NNG_SOCKET_INITIALIZER,
                          "inproc://zpp.runtime.invalid-dialer"),
            0);
  EXPECT_FALSE(dialer.valid());
}

TEST(NngRuntime, ListenerRejectsInvalidSocketOrUrl) {
  z::nng::listener listener;

  EXPECT_NE(listener.create(NNG_SOCKET_INITIALIZER, ""), 0);
  EXPECT_FALSE(listener.valid());

  EXPECT_NE(listener.create(NNG_SOCKET_INITIALIZER,
                            "inproc://zpp.runtime.invalid-listener"),
            0);
  EXPECT_FALSE(listener.valid());
}

TEST(NngRuntime, SocketPipeNotifyDispatchesCallbacks) {
  z::nng::nng runtime;
  pipe_observer_socket listener;
  z::nng::socket dialer;
  const z::nng::endpoint listener_endpoint{
      "ipc:///tmp/zpp-runtime-pipe-notify.sock",
      z::nng::transport::ipc,
      z::nng::endpoint_role::listen,
  };
  const z::nng::endpoint dialer_endpoint{
      listener_endpoint.url,
      z::nng::transport::ipc,
      z::nng::endpoint_role::dial,
  };

  ASSERT_EQ(listener.pair0_open(), 0);
  ASSERT_EQ(dialer.pair0_open(), 0);
  ASSERT_EQ(listener.pipe_notify(), 0);
  ASSERT_EQ(listener.listen(listener_endpoint), z::ERR_OK);
  ASSERT_EQ(dialer.dial(dialer_endpoint), z::ERR_OK);

  EXPECT_TRUE(wait_for_count(listener.add_pre_count, 1));
  EXPECT_TRUE(wait_for_count(listener.add_post_count, 1));

  dialer.close();
  EXPECT_TRUE(wait_for_count(listener.remove_post_count, 1));
  EXPECT_EQ(listener.pipe_notify(false), 0);
}

TEST(NngRuntime, PipeLifecycleImportantLogIsIndependentOfDebugLevel) {
  const auto runtime_path =
      std::filesystem::current_path() / "zpp_pipe_lifecycle_runtime_test.log";
  const auto important_path = std::filesystem::current_path() /
                              "zpp_pipe_lifecycle_important_test.jsonl";
  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
  auto cfg = file_log_config(runtime_path);
  cfg.async = true;
  cfg.level = spdlog::level::warn;
  cfg.important_file.enabled = true;
  cfg.important_file.file_name = important_path.string();
  cfg.important_file.max_file_size_mb = 1;
  cfg.important_file.max_files = 1;
  cfg.important_file.level = spdlog::level::warn;
  cfg.important_file.format = z::log::sink_format::jsonl;

  z::log::init(cfg);
  {
    z::nng::nng runtime;
    pipe_observer_socket listener;
    z::nng::socket dialer;
    const z::nng::endpoint listener_endpoint{
        "ipc:///tmp/zpp-pipe-lifecycle-important.sock",
        z::nng::transport::ipc,
        z::nng::endpoint_role::listen,
    };
    const z::nng::endpoint dialer_endpoint{
        listener_endpoint.url,
        z::nng::transport::ipc,
        z::nng::endpoint_role::dial,
    };

    ASSERT_EQ(listener.pair0_open(), 0);
    ASSERT_EQ(dialer.pair0_open(), 0);
    ASSERT_EQ(listener.pipe_notify(), 0);
    ASSERT_EQ(listener.listen(listener_endpoint), z::ERR_OK);
    ASSERT_EQ(dialer.dial(dialer_endpoint), z::ERR_OK);
    EXPECT_TRUE(wait_for_count(listener.add_pre_count, 1));
    EXPECT_TRUE(wait_for_count(listener.add_post_count, 1));

    dialer.close();
    EXPECT_TRUE(wait_for_count(listener.remove_post_count, 1));
    EXPECT_EQ(listener.pipe_notify(false), 0);
  }
  z::log::shutdown();

  const auto runtime_content = read_file(runtime_path);
  EXPECT_EQ(runtime_content.find("[nng:pipe-notify:"), std::string::npos);

  const auto important_content = read_file(important_path);
  EXPECT_EQ(
      count_substring(important_content, "\"component\":\"nng.pipe-notify\""),
      2U);
  EXPECT_EQ(count_substring(important_content, "\"action\":\"add-post\""), 1U);
  EXPECT_EQ(count_substring(important_content, "\"action\":\"remove-post\""),
            1U);
  EXPECT_EQ(count_substring(important_content, "\"action\":\"add-pre\""), 0U);

  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
}

TEST(NngPubSub, RejectsInvalidConfiguration) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  z::nng::subscriber subscriber;
  std::vector<z::nng::endpoint> mismatched_endpoints{
      {"inproc://zpp.runtime.publisher.invalid", z::nng::transport::tcp,
       z::nng::endpoint_role::listen},
  };

  EXPECT_NE(publisher.configure({}), z::ERR_OK);
  EXPECT_NE(publisher.configure(std::move(mismatched_endpoints)), z::ERR_OK);
  EXPECT_EQ(mismatched_endpoints.size(), 1U);
  EXPECT_NE(subscriber.configure(
                {{"inproc://zpp.runtime.subscriber.invalid",
                  z::nng::transport::inproc, z::nng::endpoint_role::dial}},
                {}),
            z::ERR_OK);
  EXPECT_TRUE(publisher.endpoints().empty());
  EXPECT_TRUE(subscriber.endpoints().empty());
}

TEST(NngPubSub, SupportsMixedRolesAndCallerManagedLifecycle) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  const auto listen_url =
      unique_inproc_url("publisher-mixed-listen", &publisher);
  const auto dial_url = unique_inproc_url("publisher-mixed-dial", &publisher);
  const z::nng::socket_options socket_config;
  const z::nng::transport_options transport_config;
  const z::nng::dialer_options dialer_config;
  const z::nng::listener_options listener_config;

  ASSERT_EQ(
      publisher.configure(
          {{listen_url, z::nng::transport::inproc,
            z::nng::endpoint_role::listen},
           {dial_url, z::nng::transport::inproc, z::nng::endpoint_role::dial}},
          &socket_config, &transport_config, &dialer_config, &listener_config),
      z::ERR_OK);
  EXPECT_EQ(publisher.endpoints().size(), 2U);
  ASSERT_EQ(publisher.start(), z::ERR_OK);

  publisher.stop();
  publisher.stop();
  EXPECT_TRUE(publisher.endpoints().empty());
  EXPECT_NE(publisher.start(), z::ERR_OK);
  EXPECT_EQ(publisher.send(nullptr), NNG_EINVAL);

  z::nng::message unsent{(size_t)0};
  ASSERT_EQ(unsent.append_u32(1), NNG_OK);
  EXPECT_EQ(publisher.send(unsent), NNG_ECLOSED);
  EXPECT_NE(unsent.msg_, nullptr);
}

TEST(NngPubSub, RollsBackWhenAnEndpointCannotStart) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  const auto duplicate_url =
      unique_inproc_url("publisher-duplicate", &publisher);

  ASSERT_EQ(publisher.configure({{duplicate_url, z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen},
                                 {duplicate_url, z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen}}),
            z::ERR_OK);
  EXPECT_NE(publisher.start(), z::ERR_OK);
  EXPECT_TRUE(publisher.endpoints().empty());
}

TEST(NngPubSub, SendsAndReceivesAcrossMultipleEndpoints) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  recording_subscriber subscriber;
  const auto first_url = unique_inproc_url("pub-sub-first", &publisher);
  const auto second_url = unique_inproc_url("pub-sub-second", &publisher);
  std::vector<z::nng::endpoint> publisher_endpoints{
      {first_url, z::nng::transport::inproc, z::nng::endpoint_role::listen},
      {second_url, z::nng::transport::inproc, z::nng::endpoint_role::listen},
  };
  std::vector<z::nng::endpoint> subscriber_endpoints{
      {first_url, z::nng::transport::inproc, z::nng::endpoint_role::dial},
      {second_url, z::nng::transport::inproc, z::nng::endpoint_role::dial},
  };

  ASSERT_EQ(publisher.configure(std::move(publisher_endpoints)), z::ERR_OK);
  ASSERT_EQ(subscriber.configure(std::move(subscriber_endpoints), {""}),
            z::ERR_OK);
  EXPECT_TRUE(publisher_endpoints.empty());
  EXPECT_TRUE(subscriber_endpoints.empty());
  ASSERT_EQ(publisher.start(), z::ERR_OK);
  ASSERT_EQ(subscriber.start(2), z::ERR_OK);
  nng_msleep(50);

  constexpr std::string_view payload{"wrapped-pub-sub"};
  for (auto attempts = 0; attempts < 100 && !subscriber.contains(payload);
       ++attempts) {
    z::nng::message outbound{(size_t)0};
    ASSERT_EQ(outbound.append(payload.data(), payload.size()), NNG_OK);
    const auto send_result = publisher.send(outbound);
    ASSERT_TRUE(send_result == NNG_OK || send_result == NNG_EAGAIN);
    if (send_result == NNG_EAGAIN) {
      EXPECT_NE(outbound.msg_, nullptr);
      nng_msleep(10);
      continue;
    }
    EXPECT_EQ(outbound.msg_, nullptr);
    nng_msleep(10);
  }
  EXPECT_TRUE(subscriber.contains(payload));

  subscriber.stop();
  publisher.stop();
}

TEST(NngPubSub, SupportsRawSendAndManagedReceive) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  recording_subscriber subscriber;
  const auto url = unique_inproc_url("pub-sub-async", &publisher);

  ASSERT_EQ(publisher.configure({{url, z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen}}),
            z::ERR_OK);
  ASSERT_EQ(subscriber.configure(
                {{url, z::nng::transport::inproc, z::nng::endpoint_role::dial}},
                {""}),
            z::ERR_OK);
  ASSERT_EQ(publisher.start(), z::ERR_OK);
  ASSERT_EQ(subscriber.start(1), z::ERR_OK);
  nng_msleep(50);

  for (auto attempts = 0; attempts < 100 && subscriber.count() == 0;
       ++attempts) {
    nng_msg *msg{nullptr};
    ASSERT_EQ(nng_msg_alloc(&msg, 0), NNG_OK);
    ASSERT_EQ(nng_msg_append_u32(msg, 42), NNG_OK);
    const auto send_result = publisher.send(msg);
    ASSERT_TRUE(send_result == NNG_OK || send_result == NNG_EAGAIN);
    nng_msleep(10);
  }

  EXPECT_TRUE(subscriber.wait_for(1));
  subscriber.stop();
  publisher.stop();
}

TEST(NngPubSubDevice, RejectsInvalidEndpoint) {
  z::nng::nng runtime;
  z::nng::pub_sub_device device;
  const z::nng::socket_options options;

  EXPECT_NE(
      device.configure(
          "invalid",
          {{"inproc://zpp.runtime.pub-sub-device.invalid",
            z::nng::transport::inproc, z::nng::endpoint_role::listen}},
          {}, &options),
      z::ERR_OK);
  EXPECT_TRUE(device.ingress_endpoints().empty());
  EXPECT_TRUE(device.egress_endpoints().empty());
}

TEST(NngPubSubDevice, RollsBackWhenAnEndpointCannotStart) {
  z::nng::nng runtime;
  z::nng::pub_sub_device device;
  const auto ingress_url =
      unique_inproc_url("pub-sub-device-rollback-ingress", &device);
  const auto duplicate_egress_url =
      unique_inproc_url("pub-sub-device-rollback-egress", &device);

  ASSERT_EQ(device.configure("rollback-test",
                             {{ingress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::dial}},
                             {{duplicate_egress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::listen},
                              {duplicate_egress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::listen}}),
            z::ERR_OK);
  EXPECT_FALSE(device.ingress_endpoints().empty());
  EXPECT_FALSE(device.egress_endpoints().empty());
  EXPECT_NE(device.start(), z::ERR_OK);
  EXPECT_TRUE(device.ingress_endpoints().empty());
  EXPECT_TRUE(device.egress_endpoints().empty());
  device.stop();
}

TEST(NngPubSubDevice, ForwardsPublisherMessagesToSubscriber) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  z::nng::pub_sub_device device;
  recording_subscriber subscriber;
  const auto ingress_url =
      unique_inproc_url("pub-sub-device-ingress", &publisher);
  const auto egress_url = unique_inproc_url("pub-sub-device-egress", &device);
  const z::nng::socket_options options;

  ASSERT_EQ(publisher.configure({{ingress_url, z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen}}),
            z::ERR_OK);
  ASSERT_EQ(subscriber.configure({{egress_url, z::nng::transport::inproc,
                                   z::nng::endpoint_role::dial}},
                                 {""}),
            z::ERR_OK);
  ASSERT_EQ(publisher.start(), z::ERR_OK);
  ASSERT_EQ(device.configure("unit-test",
                             {{ingress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::dial}},
                             {{egress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::listen}},
                             &options),
            z::ERR_OK);
  ASSERT_EQ(device.start(), z::ERR_OK);
  ASSERT_EQ(subscriber.start(1), z::ERR_OK);
  nng_msleep(50);

  constexpr std::string_view payload{"forwarded-pub-sub"};
  for (auto attempts = 0; attempts < 100 && !subscriber.contains(payload);
       ++attempts) {
    z::nng::message outbound{std::size_t{0}};
    ASSERT_EQ(outbound.append(payload.data(), payload.size()), NNG_OK);
    const int result = publisher.send(outbound);
    ASSERT_TRUE(result == NNG_OK || result == NNG_EAGAIN);
    nng_msleep(10);
  }
  EXPECT_TRUE(subscriber.contains(payload));

  subscriber.stop();
  device.stop();
  publisher.stop();
}

TEST(NngPubSubDevice, SupportsMixedIngressAndEgressEndpointRoles) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  z::nng::pub_sub_device device;
  recording_subscriber subscriber;
  const auto publisher_listen_url =
      unique_inproc_url("pub-sub-device-publisher-listen", &publisher);
  const auto device_ingress_listen_url =
      unique_inproc_url("pub-sub-device-ingress-listen", &device);
  const auto device_egress_listen_url =
      unique_inproc_url("pub-sub-device-egress-listen", &device);
  const auto subscriber_listen_url =
      unique_inproc_url("pub-sub-device-subscriber-listen", &subscriber);
  const z::nng::socket_options options;

  ASSERT_EQ(publisher.configure({{publisher_listen_url,
                                  z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen},
                                 {device_ingress_listen_url,
                                  z::nng::transport::inproc,
                                  z::nng::endpoint_role::dial}}),
            z::ERR_OK);
  ASSERT_EQ(subscriber.configure({{device_egress_listen_url,
                                   z::nng::transport::inproc,
                                   z::nng::endpoint_role::dial},
                                  {subscriber_listen_url,
                                   z::nng::transport::inproc,
                                   z::nng::endpoint_role::listen}},
                                 {""}),
            z::ERR_OK);
  ASSERT_EQ(device.configure("unit-test-mixed",
                             {{publisher_listen_url,
                               z::nng::transport::inproc,
                               z::nng::endpoint_role::dial},
                              {device_ingress_listen_url,
                               z::nng::transport::inproc,
                               z::nng::endpoint_role::listen}},
                             {{device_egress_listen_url,
                               z::nng::transport::inproc,
                               z::nng::endpoint_role::listen},
                              {subscriber_listen_url,
                               z::nng::transport::inproc,
                               z::nng::endpoint_role::dial}},
                             &options),
            z::ERR_OK);
  EXPECT_EQ(device.ingress_endpoints().size(), 2U);
  EXPECT_EQ(device.egress_endpoints().size(), 2U);

  ASSERT_EQ(subscriber.start(1), z::ERR_OK);
  ASSERT_EQ(publisher.start(), z::ERR_OK);
  ASSERT_EQ(device.start(), z::ERR_OK);
  nng_msleep(50);

  constexpr std::string_view payload{"forwarded-pub-sub-mixed"};
  for (auto attempts = 0; attempts < 100 && !subscriber.contains(payload);
       ++attempts) {
    z::nng::message outbound{std::size_t{0}};
    ASSERT_EQ(outbound.append(payload.data(), payload.size()), NNG_OK);
    const int result = publisher.send(outbound);
    ASSERT_TRUE(result == NNG_OK || result == NNG_EAGAIN);
    nng_msleep(10);
  }
  EXPECT_TRUE(subscriber.contains(payload));

  subscriber.stop();
  device.stop();
  device.stop();
  publisher.stop();
  EXPECT_TRUE(device.ingress_endpoints().empty());
  EXPECT_TRUE(device.egress_endpoints().empty());
}

TEST(NngPubSubDevice, SupportsMultipleIngressAndEgressEndpoints) {
  z::nng::nng runtime;
  z::nng::publisher publisher;
  z::nng::pub_sub_device device;
  recording_subscriber subscriber;
  const auto first_ingress_url =
      unique_inproc_url("pub-sub-device-ingress-a", &publisher);
  const auto second_ingress_url =
      unique_inproc_url("pub-sub-device-ingress-b", &publisher);
  const auto first_egress_url =
      unique_inproc_url("pub-sub-device-egress-a", &device);
  const auto second_egress_url =
      unique_inproc_url("pub-sub-device-egress-b", &device);

  ASSERT_EQ(publisher.configure({{first_ingress_url, z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen},
                                 {second_ingress_url, z::nng::transport::inproc,
                                  z::nng::endpoint_role::listen}}),
            z::ERR_OK);
  ASSERT_EQ(subscriber.configure({{second_egress_url, z::nng::transport::inproc,
                                   z::nng::endpoint_role::dial}},
                                 {""}),
            z::ERR_OK);
  ASSERT_EQ(publisher.start(), z::ERR_OK);
  ASSERT_EQ(device.configure("unit-test-multi",
                             {{first_ingress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::dial},
                              {second_ingress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::dial}},
                             {{first_egress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::listen},
                              {second_egress_url, z::nng::transport::inproc,
                               z::nng::endpoint_role::listen}}),
            z::ERR_OK);
  ASSERT_EQ(device.start(), z::ERR_OK);
  ASSERT_EQ(subscriber.start(1), z::ERR_OK);
  nng_msleep(50);

  constexpr std::string_view payload{"forwarded-pub-sub-multi"};
  for (auto attempts = 0; attempts < 100 && !subscriber.contains(payload);
       ++attempts) {
    z::nng::message outbound{std::size_t{0}};
    ASSERT_EQ(outbound.append(payload.data(), payload.size()), NNG_OK);
    const int result = publisher.send(outbound);
    ASSERT_TRUE(result == NNG_OK || result == NNG_EAGAIN);
    nng_msleep(10);
  }
  EXPECT_TRUE(subscriber.contains(payload));

  subscriber.stop();
  device.stop();
  publisher.stop();
}

TEST(NngRuntime, GlobalRuntimeConstructsAndFinalizes) {
  z::nng::nng runtime;
  SUCCEED();
}

TEST(NngRuntime, StatisticsSnapshotOwnsAndMovesSnapshot) {
  z::nng::nng runtime;
  z::nng::stats_snapshot snapshot;

  require_stats_snapshot(snapshot);
  ASSERT_TRUE(snapshot.valid());
  ASSERT_TRUE(snapshot.root().valid());

  z::nng::stats_snapshot moved(std::move(snapshot));
  EXPECT_FALSE(snapshot.valid());
  EXPECT_TRUE(moved.valid());
  EXPECT_TRUE(moved.root().valid());
}

TEST(NngRuntime, StatisticsFindsSocketAndSummarizesCommonFields) {
  z::nng::nng runtime;
  z::nng::socket socket;
  ASSERT_EQ(socket.pair0_open(), 0);

  z::nng::stats_snapshot snapshot;
  require_stats_snapshot(snapshot);

  const auto socket_stats = snapshot.find_socket(socket.get());
  ASSERT_TRUE(socket_stats.valid());

  const auto summary = z::nng::summarize_socket(socket_stats);
  ASSERT_TRUE(summary.id.has_value());
  EXPECT_EQ(*summary.id, static_cast<std::uint64_t>(socket.id()));
  EXPECT_FALSE(summary.protocol.empty());
  EXPECT_TRUE(summary.pipes.has_value());
  EXPECT_TRUE(summary.tx_msgs.has_value());
  EXPECT_TRUE(summary.rx_msgs.has_value());
}

TEST(NngRuntime, StatisticsCounterDeltaShowsConnectionEvents) {
  z::nng::nng runtime;
  z::nng::socket sender;
  z::nng::socket receiver;
  const auto url = unique_inproc_url("stats-delta", &sender);

  ASSERT_EQ(sender.pair0_open(), 0);
  ASSERT_EQ(receiver.pair0_open(), 0);

  z::nng::socket_options options;
  options.send_timeout_ms = 100;
  options.recv_timeout_ms = 100;
  ASSERT_EQ(sender.set_options(options), NNG_OK);
  ASSERT_EQ(receiver.set_options(options), NNG_OK);

  z::nng::stats_snapshot before;
  require_stats_snapshot(before);
  const auto previous = z::nng::flatten_stats(before.root());

  ASSERT_EQ(sender.listen({url, z::nng::transport::inproc,
                           z::nng::endpoint_role::listen}),
            z::ERR_OK);
  ASSERT_EQ(receiver.dial(
                {url, z::nng::transport::inproc, z::nng::endpoint_role::dial}),
            z::ERR_OK);

  constexpr std::string_view payload{"stats-payload"};
  ASSERT_TRUE(send_nonblocking(sender.get(), payload));
  ASSERT_TRUE(recv_nonblocking(receiver.get(), payload));

  z::nng::stats_snapshot after;
  require_stats_snapshot(after);
  const auto current = z::nng::flatten_stats(after.root());
  const auto deltas = z::nng::counter_delta(previous, current);

  const auto has_connection_delta =
      std::any_of(deltas.begin(), deltas.end(), [](const auto &record) {
        return (record.name == "connect" || record.name == "accept") &&
               record.numeric_value.value_or(0) > 0;
      });
  EXPECT_TRUE(has_connection_delta);
}

TEST(NngRuntime, StatisticsFlattenCopiesValuesBeyondSnapshotLifetime) {
  std::vector<z::nng::stat_record> records;
  std::string formatted;
  {
    z::nng::nng runtime;
    z::nng::socket socket;
    ASSERT_EQ(socket.pair0_open(), 0);

    z::nng::stats_snapshot snapshot;
    require_stats_snapshot(snapshot);
    records = z::nng::flatten_stats(snapshot.root());
    formatted = z::nng::format_stats_tree(snapshot.root());
  }

  ASSERT_FALSE(records.empty());
  EXPECT_FALSE(records.front().path.empty());
  EXPECT_EQ(records.front().depth, 0U);
  EXPECT_FALSE(formatted.empty());

  const auto protocol =
      std::find_if(records.begin(), records.end(), [](const auto &record) {
        return record.name == "protocol" && !record.string_value.empty();
      });
  ASSERT_NE(protocol, records.end());
  EXPECT_FALSE(protocol->path.empty());
  EXPECT_EQ(protocol->type, NNG_STAT_STRING);
  EXPECT_FALSE(protocol->string_value.empty());
}

TEST(NngRuntime, InternalLogsBridgeToZppLogger) {
  const auto path = std::filesystem::current_path() / "zpp_nng_bridge_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.pattern = "%L %v %g:%#";

  z::log::init(cfg);
  {
    z::nng::nng runtime;
    nng_log_warn("ZPPTEST", "bridge %d", 7);
  }
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find("W [nng:"), std::string::npos);
  EXPECT_NE(content.find("ZPPTEST"), std::string::npos);
  EXPECT_NE(content.find("bridge 7"), std::string::npos);
  EXPECT_NE(content.find("nng-runtime:1"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(NngRuntime, RepeatedInternalLogsAreRateLimited) {
  const auto path =
      std::filesystem::current_path() / "zpp_nng_bridge_rate_limit_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.pattern = "%L %v";

  z::log::init(cfg);
  {
    z::nng::nng runtime;
    nng_log_warn("NNG-CONN-FAIL",
                 "Failed connecting socket<42>: Connection refused");
    nng_log_warn("NNG-CONN-FAIL",
                 "Failed connecting socket<42>: Connection refused");
    nng_log_warn("NNG-CONN-FAIL",
                 "Failed connecting socket<42>: Connection refused");
    nng_log_warn("ZPPTEST", "other warning should still appear %d", 1);
    nng_log_warn("ZPPTEST", "other warning should still appear %d", 1);
    nng_log_err("ZPPTEST", "other warning should still appear %d", 1);
    nng_log_warn("ZPPTEST", "different warning should appear %d", 2);
  }
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_EQ(count_substring(content, "NNG-CONN-FAIL"), 1U);
  EXPECT_EQ(count_substring(content, "other warning should still appear 1"),
            2U);
  EXPECT_EQ(count_substring(content, "different warning should appear 2"), 1U);
  std::filesystem::remove(path);
}

TEST(NngRuntime, ProtocolStartFailureIncludesDiagnosticContext) {
  const auto path =
      std::filesystem::current_path() / "zpp_nng_protocol_error_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.pattern = "%v";

  z::log::init(cfg);
  z::nng::publisher publisher;
  EXPECT_EQ(publisher.start(), z::ERR_FAIL);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find("[nng:publisher:start]"), std::string::npos);
  EXPECT_NE(content.find("invalid socket; configure() must succeed"),
            std::string::npos);
  std::filesystem::remove(path);
}

TEST(NngRuntime, InternalLogsFollowZppRuntimeLevel) {
  const auto path =
      std::filesystem::current_path() / "zpp_nng_bridge_level_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.level = spdlog::level::warn;
  cfg.pattern = "%d %H:%M:%S.%f %t %^%L%$: %v\t%g:%#";

  z::log::init(cfg);
  {
    z::nng::nng runtime;
    nng_log_debug("ZPPTEST", "debug should not appear");
    nng_log_warn("ZPPTEST", "warn should appear");
  }
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_EQ(content.find("debug should not appear"), std::string::npos);
  EXPECT_NE(content.find("W:"), std::string::npos);
  EXPECT_NE(content.find("[nng:"), std::string::npos);
  EXPECT_NE(content.find("warn should appear"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(NngRuntime, NngErrorLogKeepsCallsiteSourceLocation) {
  const auto path =
      std::filesystem::current_path() / "zpp_nng_error_source_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.pattern = "%g:%# %v";

  z::log::init(cfg);
  nng2err(z::nng::cmp_test, z::nng::act_source_location, NNG_EINVAL);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_NE(content.find("nng_runtime_test.cpp"), std::string::npos);
  EXPECT_EQ(content.find("include/zpp/nng/log.h"), std::string::npos);
  EXPECT_NE(content.find("[nng:test:source-location]"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(NngRuntime, NngScopedInfoLogFollowsRuntimeLevel) {
  const auto path =
      std::filesystem::current_path() / "zpp_nng_scoped_info_level_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.level = spdlog::level::warn;
  cfg.pattern = "%v";

  int evaluated = 0;
  z::log::init(cfg);
  nng2inf(z::nng::cmp_test, z::nng::act_source_location, "hidden {}",
          ++evaluated);
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_EQ(evaluated, 0);
  EXPECT_EQ(content.find("hidden"), std::string::npos);
  EXPECT_EQ(content.find("[nng:test:source-location]"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(NngRuntime, NngScopedImportantLogUsesImportantChannel) {
  const auto runtime_path =
      std::filesystem::current_path() / "zpp_nng_imp_runtime_test.log";
  const auto important_path =
      std::filesystem::current_path() / "zpp_nng_imp_important_test.jsonl";
  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
  auto cfg = file_log_config(runtime_path);
  cfg.level = spdlog::level::warn;
  cfg.pattern = "%v";
  cfg.important_file.enabled = true;
  cfg.important_file.file_name = important_path.string();
  cfg.important_file.max_file_size_mb = 1;
  cfg.important_file.max_files = 1;
  cfg.important_file.level = spdlog::level::warn;
  cfg.important_file.format = z::log::sink_format::jsonl;

  int evaluated = 0;
  z::log::init(cfg);
  nng2inf(z::nng::cmp_test, z::nng::act_source_location, "hidden {}",
          ++evaluated);
  EXPECT_EQ(evaluated, 0);
  nng2imp(z::nng::cmp_test, z::nng::act_source_location, "important {}",
          ++evaluated);
  EXPECT_EQ(evaluated, 1);
  z::log::shutdown();

  const auto runtime_content = read_file(runtime_path);
  EXPECT_EQ(runtime_content.find("important 1"), std::string::npos);

  const auto lines = read_lines(important_path);
  ASSERT_EQ(lines.size(), 1);
  EXPECT_NE(lines[0].find("\"level\":\"info\""), std::string::npos);
  EXPECT_NE(lines[0].find("\"component\":\"nng.test\""), std::string::npos);
  EXPECT_NE(lines[0].find("\"action\":\"source-location\""), std::string::npos);
  EXPECT_NE(lines[0].find("[nng:test:source-location] important 1"),
            std::string::npos);

  std::filesystem::remove(runtime_path);
  std::filesystem::remove(important_path);
}

TEST(NngRuntime, NngOptionErrorSkipsSuccessfulResults) {
  const auto path =
      std::filesystem::current_path() / "zpp_nng_option_success_test.log";
  std::filesystem::remove(path);
  auto cfg = file_log_config(path);
  cfg.pattern = "%v";

  z::log::init(cfg);
  nng2opt_err(z::nng::cmp_test, z::nng::act_get_bool, NNG_OK, "test-option");
  z::log::shutdown();

  const auto content = read_file(path);
  EXPECT_EQ(content.find("test-option"), std::string::npos);
  EXPECT_EQ(content.find("[nng:test:get_bool]"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(NngRuntime, PublicHeadersCompileSmoke) {
  z::nng::socket socket;
  EXPECT_EQ(socket.pipe_notify(false), NNG_ECLOSED);
  SUCCEED();
}
