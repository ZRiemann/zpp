#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <nng/nng.h>
#include <zpp/error.h>
#include <zpp/namespace.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/socket.h>

NSB_ZPP
namespace nng {

/// Returns the current system-clock timestamp in nanoseconds since the Unix epoch.
std::int64_t now_ns();

/// Runtime options applied to NNG sockets.
struct socket_options {
    /// Send timeout in milliseconds.
    int send_timeout_ms{10};
    /// Receive timeout in milliseconds.
    int recv_timeout_ms{100};
    /// Send buffer message count.
    int send_buffer{4096};
    /// Receive buffer message count.
    int recv_buffer{4096};
    /// Enable TCP_NODELAY when supported.
    bool tcp_no_delay{true};
};

/// Publisher for normal NNG PUB sockets.
class publisher {
public:
    /// Starts a normal PUB socket listening on the provided endpoint.
    z::err_t start(const endpoint& target, const socket_options& options);
    /// Sends an owned message using non-blocking semantics.
    int send(nng_msg* message);
    /// Stops and closes the socket.
    void stop();
    /// Returns true when the publisher is running.
    bool running() const;
    /// Returns the listener URL.
    const std::string& endpoint() const;
    /// Returns the listener URL.
    const std::string& endpoint_url() const;

private:
    socket socket_;
    std::string endpoint_;
};

/// NNG raw SUB to raw PUB device forwarder.
class event_forwarder {
public:
    /// Creates an empty event forwarder.
    event_forwarder() = default;
    /// Stops the forwarder.
    ~event_forwarder();

    event_forwarder(const event_forwarder&) = delete;
    event_forwarder& operator=(const event_forwarder&) = delete;

    /// Starts the raw device with one ingress dial endpoint and one or more egress listeners.
    z::err_t start(std::string name,
                   const endpoint& ingress,
                   const std::vector<endpoint>& egress,
                   const socket_options& options);
    /// Stops sockets and AIO.
    void stop();
    /// Returns true when the forwarder owns running sockets.
    bool running() const;

private:
    static void on_device_done(void* arg);

    std::string name_;
    socket front_;
    socket back_;
    nng_aio* aio_{nullptr};
    std::atomic<bool> running_{false};
};

} // namespace nng
NSE_ZPP
