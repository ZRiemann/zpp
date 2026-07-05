#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <zpp/nng/context.h>
#include <zpp/nng/nng.h>
#include <zpp/nng/protocols/protocols.h>

namespace {

std::string protocol_url(const char *name, const void *owner) {
  std::ostringstream output;
  output << "inproc://zpp.protocol." << name << "."
         << reinterpret_cast<std::uintptr_t>(owner);
  return output.str();
}

z::nng::endpoint listen_endpoint(const std::string &url) {
  return {url, z::nng::transport::inproc, z::nng::endpoint_role::listen};
}

z::nng::endpoint dial_endpoint(const std::string &url) {
  return {url, z::nng::transport::inproc, z::nng::endpoint_role::dial};
}

template <typename Sender>
int send_until_ready(Sender &sender, std::string_view payload) {
  z::nng::message message{(size_t)0};
  if (message.append(payload.data(), payload.size()) != NNG_OK) {
    return NNG_ENOMEM;
  }
  for (int attempt = 0; attempt < 100; ++attempt) {
    const int result = sender.send(message);
    if (result != NNG_EAGAIN) {
      return result;
    }
    nng_msleep(10);
  }
  return NNG_ETIMEDOUT;
}

int send_counter_until_ready(z::nng::publisher &publisher,
                             std::uint32_t value) {
  z::nng::message message{std::size_t{0}};
  if (message.append_u32(value) != NNG_OK) {
    return NNG_ENOMEM;
  }
  for (int attempt = 0; attempt < 100; ++attempt) {
    const int result = publisher.send(message);
    if (result != NNG_EAGAIN) {
      return result;
    }
    nng_msleep(10);
  }
  return NNG_ETIMEDOUT;
}

template <typename Receiver>
int recv_until_ready(Receiver &receiver, std::string_view expected) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    z::nng::message message;
    const int result = receiver.recv(message);
    if (result == NNG_OK) {
      const std::string_view actual{static_cast<const char *>(message.body()),
                                    message.length()};
      return actual == expected ? NNG_OK : NNG_EINVAL;
    }
    if (result != NNG_EAGAIN) {
      return result;
    }
    nng_msleep(10);
  }
  return NNG_ETIMEDOUT;
}

template <typename Sender, typename Receiver>
bool aio_transfer(Sender &sender, Receiver &receiver,
                  std::string_view payload) {
  z::nng::aio send_io;
  z::nng::aio recv_io;
  if (send_io.create() != NNG_OK || recv_io.create() != NNG_OK) {
    return false;
  }
  send_io.set_timeout(1000);
  recv_io.set_timeout(1000);
  nng_msg *outbound{nullptr};
  if (nng_msg_alloc(&outbound, 0) != NNG_OK ||
      nng_msg_append(outbound, payload.data(), payload.size()) != NNG_OK) {
    nng_msg_free(outbound);
    return false;
  }
  send_io.set_msg(outbound);
  receiver.recv(recv_io);
  sender.send(send_io);
  send_io.wait();
  recv_io.wait();
  if (send_io.result() != NNG_OK || recv_io.result() != NNG_OK) {
    if (send_io.result() != NNG_OK) {
      nng_msg_free(send_io.get_msg());
      send_io.set_msg(nullptr);
    }
    return false;
  }
  nng_msg *inbound = recv_io.get_msg();
  const bool matches =
      inbound != nullptr && nng_msg_len(inbound) == payload.size() &&
      std::string_view(static_cast<const char *>(nng_msg_body(inbound)),
                       nng_msg_len(inbound)) == payload;
  nng_msg_free(inbound);
  recv_io.set_msg(nullptr);
  return matches;
}

/// Completion record captured by the managed requester tests.
struct request_completion {
  void *hint{nullptr};
  nng_err result{NNG_OK};
  std::uint32_t response{0};
  bool has_response{false};
};

/// Requester test double that records asynchronous completions.
class collecting_requester : public z::nng::requester {
public:
  /// Waits until at least count completions have arrived.
  bool wait_for(std::size_t count) {
    std::unique_lock lock{mutex_};
    return completed_.wait_for(lock, std::chrono::seconds{2}, [this, count] {
      return completion_count_ >= count;
    });
  }

  /// Returns a snapshot of all recorded completions.
  std::vector<request_completion> completions() const {
    std::lock_guard lock{mutex_};
    return {completions_.begin(), completions_.begin() + completion_count_};
  }

  /// Records a reply or terminal asynchronous error.
  void on_reply(z::nng::message &msg, nng_err result,
                void *hint) noexcept override {
    request_completion completion{hint, result};
    completion.has_response = result == NNG_OK && msg.valid() &&
                              msg.chop_u32(&completion.response) == NNG_OK;
    {
      std::lock_guard lock{mutex_};
      if (completion_count_ < completions_.size()) {
        completions_[completion_count_++] = completion;
      }
    }
    completed_.notify_all();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable completed_;
  std::array<request_completion, 16> completions_{};
  std::size_t completion_count_{0};
};

/// REP0 test service that echoes one 32-bit request value.
class echo_replier : public z::nng::replier {
protected:
  /// Receives and echoes one request.
  void on_receive(z::nng::aio_ctx &ctx, nng_err result) noexcept override {
    if (result != NNG_OK) {
      ctx.recv();
      return;
    }
    z::nng::message msg{ctx.io().release_msg()};
    std::uint32_t value{0};
    if (!msg.valid() || msg.chop_u32(&value) != NNG_OK) {
      ctx.recv();
      return;
    }
    msg.clear();
    if (msg.append_u32(value) != NNG_OK) {
      ctx.recv();
      return;
    }
    ctx.send(msg);
  }

  /// Releases failed sends and receives the next request.
  void on_send(z::nng::aio_ctx &ctx, nng_err result) noexcept override {
    if (result != NNG_OK) {
      z::nng::message release{ctx.io().release_msg()};
    }
    ctx.recv();
  }
};

/// Subscriber test double that records exact-once concurrent completions.
class collecting_subscriber : public z::nng::subscriber {
public:
  /// Waits until at least count messages have completed successfully.
  bool wait_for(std::size_t count) {
    std::unique_lock lock{mutex_};
    return completed_.wait_for(lock, std::chrono::seconds{2}, [this, count] {
      return values_.size() >= count;
    });
  }

  /// Returns a snapshot of all decoded values.
  std::vector<std::uint32_t> values() const {
    std::lock_guard lock{mutex_};
    return values_;
  }

  /// Returns the largest number of callbacks observed concurrently.
  int max_active() const noexcept {
    return max_active_.load(std::memory_order_relaxed);
  }

protected:
  /// Records one message and briefly overlaps handlers to expose AIO
  /// concurrency.
  void on_receive(z::nng::message &msg, nng_err result) noexcept override {
    if (result != NNG_OK || !msg.valid()) {
      return;
    }

    const int active = active_.fetch_add(1, std::memory_order_relaxed) + 1;
    int previous = max_active_.load(std::memory_order_relaxed);
    while (active > previous &&
           !max_active_.compare_exchange_weak(previous, active,
                                              std::memory_order_relaxed)) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    std::uint32_t value{0};
    if (msg.chop_u32(&value) == NNG_OK) {
      {
        std::lock_guard lock{mutex_};
        values_.push_back(value);
      }
      completed_.notify_all();
    }
    active_.fetch_sub(1, std::memory_order_relaxed);
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable completed_;
  std::vector<std::uint32_t> values_;
  std::atomic<int> active_{0};
  std::atomic<int> max_active_{0};
};

static_assert(std::is_same_v<decltype(std::declval<z::nng::publisher &>().send(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::subscriber &>()
                                          .start(std::declval<std::size_t>())),
                             z::err_t>);
static_assert(std::is_same_v<
              decltype(std::declval<z::nng::subscriber &>().stop()), void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::pusher &>().send(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::puller &>().recv(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::requester &>().start(
                                 std::declval<std::size_t>())),
                             z::err_t>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::replier &>().start(
                                 std::declval<std::size_t>())),
                             z::err_t>);
static_assert(
    std::is_same_v<decltype(std::declval<z::nng::requester &>().stop()), void>);
static_assert(
    std::is_same_v<decltype(std::declval<z::nng::replier &>().stop()), void>);
static_assert(std::is_same_v<
              decltype(std::declval<z::nng::requester &>().request(
                  std::declval<z::nng::message &>(), std::declval<void *>())),
              int>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::surveyor &>().send(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::respondent &>().recv(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::bus &>().send(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::pair &>().recv(
                                 std::declval<z::nng::aio &>())),
                             void>);
static_assert(std::is_move_constructible_v<z::nng::context>);
static_assert(std::is_move_assignable_v<z::nng::context>);
static_assert(!std::is_copy_constructible_v<z::nng::context>);
static_assert(!std::is_copy_assignable_v<z::nng::context>);
static_assert(std::is_same_v<decltype(std::declval<z::nng::aio_ctx &>().send(
                                 std::declval<z::nng::message &>())),
                             void>);
static_assert(
    std::is_same_v<decltype(std::declval<z::nng::aio_ctx &>().recv()), void>);

} // namespace

TEST(NngContext, OwnsAndMovesHandle) {
  z::nng::nng runtime;
  z::nng::socket requester;
  ASSERT_EQ(requester.req0_open(), NNG_OK);

  z::nng::context empty;
  EXPECT_FALSE(empty.valid());
  EXPECT_FALSE(static_cast<bool>(empty));
  EXPECT_LT(empty.id(), 0);

  ASSERT_EQ(empty.open(requester.get()), NNG_OK);
  ASSERT_TRUE(empty.valid());
  const int context_id = empty.id();

  z::nng::context moved{std::move(empty)};
  EXPECT_FALSE(empty.valid());
  ASSERT_TRUE(moved.valid());
  EXPECT_EQ(moved.id(), context_id);

  const nng_ctx released = moved.release();
  EXPECT_FALSE(moved.valid());
  z::nng::context adopted{released};
  ASSERT_TRUE(adopted.valid());

  z::nng::context reset;
  reset.reset(adopted.release());
  EXPECT_FALSE(adopted.valid());
  ASSERT_TRUE(reset.valid());
  EXPECT_EQ(reset.id(), context_id);
  reset.close();
  reset.close();
  EXPECT_FALSE(reset.valid());

  z::nng::socket publisher;
  ASSERT_EQ(publisher.pub0_open(), NNG_OK);
  z::nng::context unsupported;
  EXPECT_EQ(unsupported.open(publisher.get()), NNG_ENOTSUP);
  EXPECT_FALSE(unsupported.valid());

  z::nng::context_options unchanged{17, 23};
  EXPECT_NE(unsupported.get_options(unchanged), NNG_OK);
  EXPECT_EQ(unchanged.send_timeout_ms, 17);
  EXPECT_EQ(unchanged.recv_timeout_ms, 23);
}

TEST(NngContext, AioMovesUserHint) {
  z::nng::nng runtime;
  int first_hint{1};
  z::nng::aio source;
  ASSERT_EQ(source.create(), NNG_OK);
  source.set_hint(&first_hint);
  source.set_state(z::nng::aio::state::recv);

  z::nng::aio moved{std::move(source)};
  EXPECT_FALSE(source.valid());
  EXPECT_EQ(source.get_hint(), nullptr);
  EXPECT_EQ(source.get_state(), z::nng::aio::state::done);
  EXPECT_TRUE(moved.valid());
  EXPECT_EQ(moved.get_hint(), &first_hint);
  EXPECT_EQ(moved.get_state(), z::nng::aio::state::recv);

  int second_hint{2};
  z::nng::aio assigned;
  ASSERT_EQ(assigned.create(), NNG_OK);
  assigned.set_hint(&second_hint);
  assigned = std::move(moved);

  EXPECT_FALSE(moved.valid());
  EXPECT_EQ(moved.get_hint(), nullptr);
  EXPECT_EQ(moved.get_state(), z::nng::aio::state::done);
  EXPECT_TRUE(assigned.valid());
  EXPECT_EQ(assigned.get_hint(), &first_hint);
  EXPECT_EQ(assigned.get_state(), z::nng::aio::state::recv);
}

TEST(NngContext, AioContextSupportsWaitWithoutCallback) {
  z::nng::nng runtime;
  z::nng::socket requester;
  ASSERT_EQ(requester.req0_open(), NNG_OK);

  z::nng::aio_ctx context;
  ASSERT_EQ(context.open(requester.get(), nullptr, nullptr), NNG_OK);
  context.io().sleep(1);
  context.io().wait();
  EXPECT_EQ(context.io().result(), NNG_OK);
  context.close();
}

TEST(NngContext, TransfersMessagesAndAppliesOptions) {
  z::nng::nng runtime;
  z::nng::socket requester;
  z::nng::socket replier;
  ASSERT_EQ(requester.req0_open(), NNG_OK);
  ASSERT_EQ(replier.rep0_open(), NNG_OK);

  const auto url = protocol_url("context-wrapper", &requester);
  ASSERT_EQ(replier.listen(listen_endpoint(url)), z::ERR_OK);
  ASSERT_EQ(requester.dial(dial_endpoint(url), false), z::ERR_OK);

  z::nng::context request_context;
  z::nng::context reply_context;
  ASSERT_EQ(request_context.open(requester.get()), NNG_OK);
  ASSERT_EQ(reply_context.open(replier.get()), NNG_OK);

  const z::nng::context_options expected_options{125, 375};
  ASSERT_EQ(request_context.set_options(expected_options), NNG_OK);
  z::nng::context_options actual_options;
  ASSERT_EQ(request_context.get_options(actual_options), NNG_OK);
  EXPECT_EQ(actual_options.send_timeout_ms, expected_options.send_timeout_ms);
  EXPECT_EQ(actual_options.recv_timeout_ms, expected_options.recv_timeout_ms);

  ASSERT_EQ(request_context.set_ms(NNG_OPT_REQ_RESENDTIME, 250), NNG_OK);
  nng_duration resend_time{0};
  ASSERT_EQ(request_context.get_ms(NNG_OPT_REQ_RESENDTIME, &resend_time),
            NNG_OK);
  EXPECT_EQ(resend_time, 250);

  z::nng::message request{(size_t)0};
  ASSERT_EQ(request.append("context-request", 15), NNG_OK);
  ASSERT_EQ(request_context.send(request, 0), NNG_OK);
  EXPECT_FALSE(request.valid());

  z::nng::message incoming;
  ASSERT_EQ(reply_context.recv(incoming, 0), NNG_OK);
  ASSERT_TRUE(incoming.valid());
  EXPECT_EQ(std::string_view(static_cast<const char *>(incoming.body()),
                             incoming.length()),
            "context-request");

  z::nng::aio send_io;
  z::nng::aio recv_io;
  ASSERT_EQ(send_io.create(), NNG_OK);
  ASSERT_EQ(recv_io.create(), NNG_OK);
  send_io.set_timeout(1000);
  recv_io.set_timeout(1000);
  send_io.set_msg(incoming.release());

  request_context.recv(recv_io);
  reply_context.send(send_io);
  EXPECT_EQ(recv_io.get_state(), z::nng::aio::state::recv);
  EXPECT_EQ(send_io.get_state(), z::nng::aio::state::send);
  send_io.wait();
  recv_io.wait();

  const auto send_result = send_io.result();
  if (send_result != NNG_OK) {
    if (auto *unsent = send_io.release_msg(); unsent != nullptr) {
      nng_msg_free(unsent);
    }
  }
  ASSERT_EQ(send_result, NNG_OK);
  ASSERT_EQ(recv_io.result(), NNG_OK);
  z::nng::message reply{recv_io.release_msg()};
  ASSERT_TRUE(reply.valid());
  EXPECT_EQ(
      std::string_view(static_cast<const char *>(reply.body()), reply.length()),
      "context-request");

  request_context.close();
  reply_context.close();
}

TEST(NngProtocols, PushPullSynchronousTransfer) {
  z::nng::nng runtime;
  z::nng::pusher push;
  z::nng::puller pull;
  const auto url = protocol_url("pipeline", &push);

  ASSERT_EQ(pull.configure({listen_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(push.configure({dial_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(pull.start(), z::ERR_OK);
  ASSERT_EQ(push.start(), z::ERR_OK);
  ASSERT_EQ(send_until_ready(push, "pipeline"), NNG_OK);
  EXPECT_EQ(recv_until_ready(pull, "pipeline"), NNG_OK);
}

TEST(NngProtocols, SurveyRespondentOrderingAndTransfer) {
  z::nng::nng runtime;
  z::nng::surveyor survey;
  z::nng::respondent response;
  const auto url = protocol_url("survey", &survey);

  ASSERT_EQ(response.configure({listen_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(survey.configure({dial_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(response.start(), z::ERR_OK);
  ASSERT_EQ(survey.start(), z::ERR_OK);
  nng_msleep(50);

  ASSERT_EQ(send_until_ready(survey, "survey"), NNG_OK);
  z::nng::message incoming;
  int receive_result{NNG_EAGAIN};
  for (int attempt = 0; attempt < 100 && receive_result == NNG_EAGAIN;
       ++attempt) {
    receive_result = response.recv(incoming);
    if (receive_result == NNG_EAGAIN) {
      nng_msleep(10);
    }
  }
  ASSERT_EQ(receive_result, NNG_OK);
  incoming.clear();
  ASSERT_EQ(incoming.append("response", 8), NNG_OK);
  z::nng::aio survey_receive;
  ASSERT_EQ(survey_receive.create(), NNG_OK);
  survey.recv(survey_receive);
  ASSERT_EQ(response.send(incoming, 0), NNG_OK);
  survey_receive.wait();
  ASSERT_EQ(survey_receive.result(), NNG_OK);
  nng_msg *response_message = survey_receive.get_msg();
  ASSERT_NE(response_message, nullptr);
  EXPECT_EQ(std::string_view(
                static_cast<const char *>(nng_msg_body(response_message)),
                nng_msg_len(response_message)),
            "response");
  nng_msg_free(response_message);
  survey_receive.set_msg(nullptr);
}

TEST(NngProtocols, BusAndBothPairVersionsTransfer) {
  z::nng::nng runtime;
  z::nng::bus first_bus;
  z::nng::bus second_bus;
  const auto bus_url = protocol_url("bus", &first_bus);
  ASSERT_EQ(first_bus.configure({listen_endpoint(bus_url)}), z::ERR_OK);
  ASSERT_EQ(second_bus.configure({dial_endpoint(bus_url)}), z::ERR_OK);
  ASSERT_EQ(first_bus.start(), z::ERR_OK);
  ASSERT_EQ(second_bus.start(), z::ERR_OK);
  nng_msleep(50);
  z::nng::message bus_message{(size_t)0};
  ASSERT_EQ(bus_message.append("bus", 3), NNG_OK);
  ASSERT_EQ(second_bus.send(bus_message, 0), NNG_OK);
  EXPECT_EQ(recv_until_ready(first_bus, "bus"), NNG_OK);

  for (const auto version :
       {z::nng::pair_version::v0, z::nng::pair_version::v1}) {
    z::nng::pair first;
    z::nng::pair second;
    const auto pair_url = protocol_url(
        version == z::nng::pair_version::v0 ? "pair0" : "pair1", &first);
    ASSERT_EQ(first.configure(listen_endpoint(pair_url), version), z::ERR_OK);
    ASSERT_EQ(second.configure(dial_endpoint(pair_url), version), z::ERR_OK);
    ASSERT_EQ(first.start(), z::ERR_OK);
    ASSERT_EQ(second.start(), z::ERR_OK);
    ASSERT_EQ(send_until_ready(first, "pair"), NNG_OK);
    EXPECT_EQ(recv_until_ready(second, "pair"), NNG_OK);
  }
}

TEST(NngProtocols, SurveyProtocolsOpenIndependentContexts) {
  z::nng::nng runtime;

  z::nng::surveyor surveyor;
  const auto survey_url = protocol_url("survey-context", &surveyor);
  ASSERT_EQ(surveyor.configure({listen_endpoint(survey_url)}), z::ERR_OK);
  z::nng::aio_ctx survey_context;
  ASSERT_EQ(surveyor.open_context(survey_context), NNG_OK);
  survey_context.close();
  surveyor.stop();

  z::nng::respondent respondent;
  const auto respondent_url = protocol_url("respondent-context", &respondent);
  ASSERT_EQ(respondent.configure({listen_endpoint(respondent_url)}), z::ERR_OK);
  z::nng::aio_ctx respondent_context;
  ASSERT_EQ(respondent.open_context(respondent_context), NNG_OK);
  respondent_context.close();
  respondent.stop();
}

TEST(NngProtocols, ManagedSubscriberDeliversEachMessageOnceAcrossThreeAios) {
  z::nng::nng runtime;
  std::array<z::nng::publisher, 3> publishers;
  collecting_subscriber subscriber;
  std::vector<z::nng::endpoint> subscriber_endpoints;

  for (auto &publisher : publishers) {
    const auto url = protocol_url("managed-subscriber", &publisher);
    ASSERT_EQ(publisher.configure({listen_endpoint(url)}), z::ERR_OK);
    subscriber_endpoints.push_back(dial_endpoint(url));
    ASSERT_EQ(publisher.start(), z::ERR_OK);
  }
  ASSERT_EQ(subscriber.configure(std::move(subscriber_endpoints), {""}),
            z::ERR_OK);
  ASSERT_EQ(subscriber.start(3), z::ERR_OK);
  nng_msleep(50);

  constexpr std::size_t message_count{12};
  for (std::uint32_t value = 0; value < message_count; ++value) {
    ASSERT_EQ(
        send_counter_until_ready(publishers[value % publishers.size()], value),
        NNG_OK);
  }
  ASSERT_TRUE(subscriber.wait_for(message_count));

  auto values = subscriber.values();
  ASSERT_EQ(values.size(), message_count);
  std::sort(values.begin(), values.end());
  for (std::uint32_t value = 0; value < message_count; ++value) {
    EXPECT_EQ(values[value], value);
  }
  EXPECT_GE(subscriber.max_active(), 2);

  subscriber.stop();
  for (auto &publisher : publishers) {
    publisher.stop();
  }
}

TEST(NngProtocols, ManagedSubscriberSuppressesPendingStopCompletions) {
  z::nng::nng runtime;
  collecting_subscriber subscriber;
  const auto url = protocol_url("managed-subscriber-stop", &subscriber);
  z::nng::socket_options options;
  options.recv_timeout_ms = NNG_DURATION_INFINITE;

  ASSERT_EQ(
      subscriber.configure({listen_endpoint(url)}, {""}, nullptr, &options),
      z::ERR_OK);
  ASSERT_EQ(subscriber.start(3), z::ERR_OK);
  subscriber.stop();

  EXPECT_TRUE(subscriber.values().empty());
}

TEST(NngProtocols, ManagedRequestReplyCorrelatesThreeConcurrentRequests) {
  z::nng::nng runtime;
  collecting_requester requester;
  echo_replier replier;
  const auto url = protocol_url("managed-req-rep", &requester);

  ASSERT_EQ(replier.configure({listen_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(requester.configure({dial_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(replier.start(3), z::ERR_OK);
  ASSERT_EQ(requester.start(3), z::ERR_OK);

  std::array<std::uint32_t, 6> hints{11, 22, 33, 44, 55, 66};
  for (std::size_t round = 0; round < 2; ++round) {
    for (std::size_t index = 0; index < 3; ++index) {
      auto &hint = hints[(round * 3) + index];
      z::nng::message request{std::size_t{0}};
      ASSERT_EQ(request.append_u32(hint), NNG_OK);
      ASSERT_EQ(requester.request(request, &hint), NNG_OK);
      EXPECT_FALSE(request.valid());
    }
    ASSERT_TRUE(requester.wait_for((round + 1) * 3));
  }

  const auto completions = requester.completions();
  ASSERT_EQ(completions.size(), hints.size());
  for (auto &hint : hints) {
    const auto completion =
        std::find_if(completions.begin(), completions.end(),
                     [&hint](const auto &item) { return item.hint == &hint; });
    ASSERT_NE(completion, completions.end());
    EXPECT_EQ(completion->result, NNG_OK);
    EXPECT_TRUE(completion->has_response);
    EXPECT_EQ(completion->response, hint);
  }

  requester.stop();
  replier.stop();
}

TEST(NngProtocols,
     ManagedRequesterAppliesBackpressureAndCompletesPendingStops) {
  z::nng::nng runtime;
  collecting_requester requester;
  const auto url = protocol_url("managed-req-stop", &requester);

  ASSERT_EQ(requester.configure({listen_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(requester.start(3), z::ERR_OK);

  std::array<std::uint32_t, 3> hints{1, 2, 3};
  for (auto &hint : hints) {
    z::nng::message request{std::size_t{0}};
    ASSERT_EQ(request.append_u32(hint), NNG_OK);
    ASSERT_EQ(requester.request(request, &hint), NNG_OK);
    EXPECT_FALSE(request.valid());
  }

  z::nng::message overflow{std::size_t{0}};
  ASSERT_EQ(overflow.append_u32(4), NNG_OK);
  EXPECT_EQ(requester.request(overflow), NNG_EAGAIN);
  EXPECT_TRUE(overflow.valid());

  requester.stop();
  const auto completions = requester.completions();
  ASSERT_EQ(completions.size(), hints.size());
  for (const auto &completion : completions) {
    EXPECT_NE(completion.result, NNG_OK);
    EXPECT_FALSE(completion.has_response);
    EXPECT_NE(completion.hint, nullptr);
  }
}

TEST(NngProtocols, StatelessProtocolsSupportAioTransfer) {
  z::nng::nng runtime;

  z::nng::pusher pusher;
  z::nng::puller puller;
  const auto pipeline_url = protocol_url("pipeline-aio", &pusher);
  ASSERT_EQ(puller.configure({listen_endpoint(pipeline_url)}), z::ERR_OK);
  ASSERT_EQ(pusher.configure({dial_endpoint(pipeline_url)}), z::ERR_OK);
  ASSERT_EQ(puller.start(), z::ERR_OK);
  ASSERT_EQ(pusher.start(), z::ERR_OK);
  EXPECT_TRUE(aio_transfer(pusher, puller, "pipeline-aio"));

  z::nng::bus first_bus;
  z::nng::bus second_bus;
  const auto bus_url = protocol_url("bus-aio", &first_bus);
  ASSERT_EQ(first_bus.configure({listen_endpoint(bus_url)}), z::ERR_OK);
  ASSERT_EQ(second_bus.configure({dial_endpoint(bus_url)}), z::ERR_OK);
  ASSERT_EQ(first_bus.start(), z::ERR_OK);
  ASSERT_EQ(second_bus.start(), z::ERR_OK);
  nng_msleep(50);
  EXPECT_TRUE(aio_transfer(second_bus, first_bus, "bus-aio"));

  z::nng::pair first_pair;
  z::nng::pair second_pair;
  const auto pair_url = protocol_url("pair-aio", &first_pair);
  ASSERT_EQ(first_pair.configure(listen_endpoint(pair_url)), z::ERR_OK);
  ASSERT_EQ(second_pair.configure(dial_endpoint(pair_url)), z::ERR_OK);
  ASSERT_EQ(first_pair.start(), z::ERR_OK);
  ASSERT_EQ(second_pair.start(), z::ERR_OK);
  EXPECT_TRUE(aio_transfer(first_pair, second_pair, "pair-aio"));
}

TEST(NngProtocols, SurveyRespondentSupportsAioStateSequence) {
  z::nng::nng runtime;
  z::nng::surveyor surveyor;
  z::nng::respondent respondent;
  const auto url = protocol_url("survey-aio", &surveyor);
  ASSERT_EQ(respondent.configure({listen_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(surveyor.configure({dial_endpoint(url)}), z::ERR_OK);
  ASSERT_EQ(respondent.start(), z::ERR_OK);
  ASSERT_EQ(surveyor.start(), z::ERR_OK);
  nng_msleep(50);

  z::nng::aio survey_send;
  z::nng::aio response_recv;
  ASSERT_EQ(survey_send.create(), NNG_OK);
  ASSERT_EQ(response_recv.create(), NNG_OK);
  survey_send.set_timeout(1000);
  response_recv.set_timeout(1000);
  nng_msg *survey{nullptr};
  ASSERT_EQ(nng_msg_alloc(&survey, 0), NNG_OK);
  ASSERT_EQ(nng_msg_append(survey, "aio-survey", 10), NNG_OK);
  survey_send.set_msg(survey);
  respondent.recv(response_recv);
  surveyor.send(survey_send);
  survey_send.wait();
  response_recv.wait();
  ASSERT_EQ(survey_send.result(), NNG_OK);
  ASSERT_EQ(response_recv.result(), NNG_OK);

  nng_msg *response = response_recv.get_msg();
  response_recv.set_msg(nullptr);
  ASSERT_NE(response, nullptr);
  nng_msg_clear(response);
  ASSERT_EQ(nng_msg_append(response, "aio-response", 12), NNG_OK);
  z::nng::aio response_send;
  z::nng::aio survey_recv;
  ASSERT_EQ(response_send.create(), NNG_OK);
  ASSERT_EQ(survey_recv.create(), NNG_OK);
  response_send.set_timeout(1000);
  survey_recv.set_timeout(1000);
  response_send.set_msg(response);
  surveyor.recv(survey_recv);
  respondent.send(response_send);
  response_send.wait();
  survey_recv.wait();
  ASSERT_EQ(response_send.result(), NNG_OK);
  ASSERT_EQ(survey_recv.result(), NNG_OK);
  nng_msg *received = survey_recv.get_msg();
  ASSERT_NE(received, nullptr);
  EXPECT_EQ(std::string_view(static_cast<const char *>(nng_msg_body(received)),
                             nng_msg_len(received)),
            "aio-response");
  nng_msg_free(received);
  survey_recv.set_msg(nullptr);
}
