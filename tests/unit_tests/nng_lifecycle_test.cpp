#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <zpp/core/lifecycle.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/message.h>
#include <zpp/nng/nng.h>
#include <zpp/nng/protocols/replier.h>
#include <zpp/nng/protocols/requester.h>

using namespace std::chrono_literals;

namespace {

class collecting_requester final : public z::nng::requester {
public:
  /// Waits for one request completion and returns the decoded value.
  bool wait(std::uint32_t &value) {
    std::unique_lock lock{mutex_};
    if (!completed_.wait_for(lock, 2s, [this] { return done_; })) {
      return false;
    }
    value = value_;
    return succeeded_;
  }

protected:
  /// Records one asynchronous request completion.
  void on_reply(z::nng::message &message, nng_err result,
                void *) noexcept override {
    std::uint32_t value{0};
    const bool succeeded =
        result == NNG_OK && message.valid() &&
        message.chop_u32(&value) == NNG_OK;
    {
      std::lock_guard lock{mutex_};
      value_ = value;
      succeeded_ = succeeded;
      done_ = true;
    }
    completed_.notify_all();
  }

private:
  std::mutex mutex_;
  std::condition_variable completed_;
  std::uint32_t value_{0};
  bool succeeded_{false};
  bool done_{false};
};

class lifecycle_replier final : public z::nng::replier {
public:
  /// Starts lifecycle admission after the managed REP contexts are active.
  bool start_lifecycle() noexcept { return lifecycle_.start(); }

  /// Prevents new deferred work from being admitted.
  void quiesce() noexcept { lifecycle_.quiesce(); }

  /// Waits for admitted deferred work to finish.
  bool wait_drained_for(std::chrono::milliseconds timeout) {
    return lifecycle_.wait_drained_for(timeout);
  }

  /// Marks the lifecycle stopped after the REP transport is stopped.
  bool stop_lifecycle() noexcept { return lifecycle_.stop(); }

  /// Waits until one request has been admitted for deferred processing.
  bool wait_admitted() {
    std::unique_lock lock{mutex_};
    return admitted_cv_.wait_for(lock, 2s, [this] { return admitted_; });
  }

  /// Joins the test worker used to prove cross-thread deferred replies.
  void join_worker() {
    std::thread worker;
    {
      std::lock_guard lock{mutex_};
      worker = std::move(worker_);
    }
    if (worker.joinable()) {
      worker.join();
    }
  }

protected:
  /// Defers an admitted reply to another thread while retaining the context
  /// only as a non-owning borrow protected by lifecycle::guard.
  void on_receive(z::nng::aio_ctx &ctx, nng_err result) noexcept override {
    if (result != NNG_OK) {
      ctx.recv();
      return;
    }

    z::nng::message request{ctx.io().release_msg()};
    std::uint32_t value{0};
    if (!request.valid() || request.chop_u32(&value) != NNG_OK) {
      z::nng::message invalid{std::size_t{0}};
      if (invalid.valid() && invalid.append_u32(0) == NNG_OK) {
        ctx.send(invalid);
      }
      return;
    }

    auto activity = lifecycle_.try_acquire();
    if (!activity) {
      z::nng::message unavailable{std::size_t{0}};
      if (unavailable.valid() && unavailable.append_u32(0) == NNG_OK) {
        ctx.send(unavailable);
      }
      return;
    }

    {
      std::lock_guard lock{mutex_};
      activity_.emplace(std::move(activity));
      worker_ = std::thread([context = &ctx, value] {
        std::this_thread::sleep_for(50ms);
        z::nng::message reply{std::size_t{0}};
        if (reply.valid() && reply.append_u32(value + 1) == NNG_OK) {
          context->send(reply);
        }
      });
      admitted_ = true;
    }
    admitted_cv_.notify_all();
  }

  /// Releases the lifecycle activity only after asynchronous send completion.
  void on_send(z::nng::aio_ctx &ctx, nng_err result) noexcept override {
    if (result != NNG_OK) {
      z::nng::message release{ctx.io().release_msg()};
    }
    {
      std::lock_guard lock{mutex_};
      activity_.reset();
    }
    ctx.recv();
  }

private:
  z::lifecycle lifecycle_;
  std::mutex mutex_;
  std::condition_variable admitted_cv_;
  std::optional<z::lifecycle::guard> activity_;
  std::thread worker_;
  bool admitted_{false};
};

TEST(NngLifecycle, DeferredReplyDrainsBeforeReplierStop) {
  constexpr char url[] = "inproc://zpp.lifecycle.deferred-reply";

  z::nng::nng runtime;
  lifecycle_replier server;
  std::vector<z::nng::endpoint> server_endpoints{
      {url, z::nng::transport::inproc, z::nng::endpoint_role::listen}};
  ASSERT_EQ(server.configure(std::move(server_endpoints)), z::ERR_OK);
  ASSERT_EQ(server.start(1), z::ERR_OK);
  ASSERT_TRUE(server.start_lifecycle());

  collecting_requester client;
  std::vector<z::nng::endpoint> client_endpoints{
      {url, z::nng::transport::inproc, z::nng::endpoint_role::dial}};
  ASSERT_EQ(client.configure(std::move(client_endpoints)), z::ERR_OK);
  ASSERT_EQ(client.start(1), z::ERR_OK);

  z::nng::message request{std::size_t{0}};
  ASSERT_TRUE(request.valid());
  ASSERT_EQ(request.append_u32(41), NNG_OK);
  ASSERT_EQ(client.request(request), NNG_OK);

  ASSERT_TRUE(server.wait_admitted());
  server.quiesce();
  EXPECT_FALSE(server.wait_drained_for(1ms));

  std::uint32_t reply{0};
  ASSERT_TRUE(client.wait(reply));
  EXPECT_EQ(reply, 42U);

  server.join_worker();
  EXPECT_TRUE(server.wait_drained_for(1s));

  client.stop();
  server.stop();
  EXPECT_TRUE(server.stop_lifecycle());
}

} // namespace
