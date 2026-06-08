#pragma once

#include "aio.h"
#include "defs.h"
#include "message.h"

#include <cstddef>
#include <cstdint>

#include <zpp/error.h>
#include <zpp/spdlog.h>

NSB_NNG

struct endpoint;
struct socket_options;

/// RAII wrapper for an NNG socket.
class socket {
public:
    /// Constructs an empty socket.
    socket() = default;
    /// Takes ownership of an already opened NNG socket.
    explicit socket(nng_socket socket);
    /// Closes the owned socket.
    virtual ~socket();

    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    /// Moves a socket.
    socket(socket&& other) noexcept;
    /// Moves a socket.
    socket& operator=(socket&& other) noexcept;

    /// Sends a message and transfers ownership on success.
    int send(message& msg, int flags = NNG_FLAG_NONBLOCK) {
        const auto ret = nng_sendmsg(sock_, msg.msg_, flags);
        if (ret != 0) {
            // ret = NNG_EAGAIN is normal in non-blocking mode
            spd_err("nng_send failed: {}", strerr(ret));
        } else {
            msg.owned_ = false;
        }
        return ret;
    }

    /// Starts asynchronous send on this socket.
    void send(aio& io) {
        nng_socket_send(sock_, io.aio_);
    }

    /// Receives a message into the wrapper.
    int recv(message& msg, int flags = 0) {
        const auto ret = nng_recvmsg(sock_, &msg.msg_, flags);
        if (ret != 0) {
            // flags = 0 blocks until a message arrives.
            spd_err("nng_recv failed: {}", strerr(ret));
            msg.owned_ = false;
        }
        return ret;
    }

    /// Starts asynchronous receive on this socket.
    void recv(aio& io) {
        nng_socket_recv(sock_, io.aio_);
    }

    /// Returns true when the wrapper owns a valid socket.
    bool valid() const;
    /// Returns the underlying socket.
    nng_socket get() const;
    /// Closes and clears the socket.
    void close();
    /// Replaces the owned socket.
    void reset(nng_socket socket);

    /// Applies common runtime options to this socket.
    void apply_options(const socket_options& options);
    /// Starts a listener for the endpoint and removes stale ipc sockets first.
    z::err_t listen(const endpoint& target);
    /// Starts a dialer for the endpoint (non-blocking by default).
    z::err_t dial(const endpoint& target, bool nonblock = true);

    /// Returns the NNG socket id.
    int id() const {
        return nng_socket_id(sock_);
    }

    /// Returns whether the socket is raw.
    int raw(bool* raw) {
        return nng_socket_raw(sock_, raw);
    }

    /// Returns the protocol id.
    int proto_id(std::uint16_t* idp) {
        return nng_socket_proto_id(sock_, idp);
    }

    /// Returns the peer protocol id.
    int peer_id(std::uint16_t* idp) {
        return nng_socket_peer_id(sock_, idp);
    }

    /// Returns the protocol name.
    int proto_name(const char** name) {
        return nng_socket_proto_name(sock_, name);
    }

    /// Returns the peer protocol name.
    int peer_name(const char** name) {
        return nng_socket_peer_name(sock_, name);
    }

    /// Opens a BUS0 socket.
    int bus0_open();
    /// Opens a PUB0 socket.
    int pub0_open();
    /// Opens a raw PUB0 socket.
    int pub0_open_raw();
    /// Opens a SUB0 socket.
    int sub0_open();
    /// Opens a raw SUB0 socket.
    int sub0_open_raw();
    /// Opens a PULL0 socket.
    int pull0_open();
    /// Opens a PUSH0 socket.
    int push0_open();
    /// Opens a REP0 socket.
    int rep0_open();
    /// Opens a REQ0 socket.
    int req0_open();
    /// Opens a RESPONDENT0 socket.
    int respondent0_open();
    /// Opens a SURVEYOR0 socket.
    int surveyor0_open();
    /// Opens a PAIR0 socket.
    int pair0_open();
    /// Opens a PAIR0 socket.
    int pare0_open() {
        return pair0_open();
    }
    /// Opens a PAIR1 socket.
    int pair1_open();
    /// Opens a polyamorous PAIR1 socket.
    int pair1_open_poly();
    /// Opens a polyamorous PAIR1 socket.
    int pare1_open_poly() {
        return pair1_open_poly();
    }

    /// Sets a boolean socket option.
    int set_bool(const char* name, bool v) {
        const auto ret = nng_socket_set_bool(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_set_bool failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Gets a boolean socket option.
    int get_bool(const char* name, bool* v) {
        const auto ret = nng_socket_get_bool(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_get_bool failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Sets an integer socket option.
    int set_int(const char* name, int v) {
        const auto ret = nng_socket_set_int(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_set_int failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Gets an integer socket option.
    int get_int(const char* name, int* v) {
        const auto ret = nng_socket_get_int(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_get_int failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Sets a size socket option.
    int set_size(const char* name, std::size_t v) {
        const auto ret = nng_socket_set_size(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_set_size failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Gets a size socket option.
    int get_size(const char* name, std::size_t* v) {
        const auto ret = nng_socket_get_size(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_get_size failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Sets a duration socket option in milliseconds.
    int set_ms(const char* name, nng_duration v) {
        const auto ret = nng_socket_set_ms(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_set_ms failed: {}", strerr(ret));
        }
        return ret;
    }

    /// Gets a duration socket option in milliseconds.
    int get_ms(const char* name, nng_duration* v) {
        const auto ret = nng_socket_get_ms(sock_, name, v);
        if (ret != 0) {
            spd_err("nng_socket_get_ms failed: {}", strerr(ret));
        }
        return ret;
    }

protected:
    nng_socket sock_{NNG_SOCKET_INITIALIZER};

private:
    bool valid_{false};
};

NSE_NNG
