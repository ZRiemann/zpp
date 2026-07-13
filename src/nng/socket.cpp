#include <zpp/nng/socket.h>

#include <cstddef>
#include <string_view>

#include <zpp/nng/endpoint.h>
#include <zpp/nng/log.h>
#include <zpp/nng/pipe.h>

namespace z::nng {
namespace {

int open_socket(socket &target, std::string_view context,
                int (*open_fn)(nng_socket *)) {
  nng_socket opened{};
  const auto rv = open_fn(&opened);
  if (rv != 0) {
    nng2errf(cmp_socket, context, "id={} error={} message={}", target.id(), rv,
             nng_strerror(static_cast<::nng_err>(rv)));
    return rv;
  }
  target.reset(opened);
  nng2inf(cmp_socket, context, "id={}", target.id());
  return rv;
}

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
void log_pipe_options(pipe &observed_pipe) {
  bool bool_value{};
  observed_pipe.get_bool(NNG_OPT_TLS_VERIFIED, bool_value);
  observed_pipe.get_bool(NNG_OPT_TCP_NODELAY, bool_value);
  observed_pipe.get_bool(NNG_OPT_TCP_KEEPALIVE, bool_value);

  const char *string_value{nullptr};
  observed_pipe.get_string(NNG_OPT_TLS_PEER_CN, string_value);

  int int_value{};
  observed_pipe.get_int(NNG_OPT_PEER_UID, int_value);
  observed_pipe.get_int(NNG_OPT_PEER_GID, int_value);
  observed_pipe.get_int(NNG_OPT_PEER_PID, int_value);
  observed_pipe.get_int(NNG_OPT_PEER_ZONEID, int_value);

  std::size_t size_value{};
  observed_pipe.get_size(NNG_OPT_RECVMAXSZ, size_value);

  nng_duration duration_value{};
  observed_pipe.get_ms(NNG_OPT_RECONNMINT, duration_value);
  observed_pipe.get_ms(NNG_OPT_RECONNMAXT, duration_value);
}
#endif

void log_pipe_event(std::string_view event, pipe &observed_pipe,
                    bool is_important) {
  if (is_important) {
    nng2imp(cmp_pipe_notify, event,
            "id={} pipe={} socket={} listener={} dialer={} self={} peer={}",
            nng_socket_id(observed_pipe.socket()), observed_pipe.id(),
            nng_socket_id(observed_pipe.socket()),
            nng_listener_id(observed_pipe.listener()),
            nng_dialer_id(observed_pipe.dialer()),
            observed_pipe.get_address(false), observed_pipe.get_address(true));
  } else {
    nng2dbg(cmp_pipe_notify, event,
            "id={} pipe={} socket={} listener={} dialer={} self={} peer={}",
            nng_socket_id(observed_pipe.socket()), observed_pipe.id(),
            nng_socket_id(observed_pipe.socket()),
            nng_listener_id(observed_pipe.listener()),
            nng_dialer_id(observed_pipe.dialer()),
            observed_pipe.get_address(false), observed_pipe.get_address(true));
  }

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
  if (event == "add-post") {
    if (spd_should_log(spdlog::level::debug)) {
      log_pipe_options(observed_pipe);
    }
  }
#else
  (void)event;
  (void)observed_pipe;
#endif
}

} // namespace

socket::socket(nng_socket opened) : sock_(opened) {}

socket::~socket() { close(); }

socket::socket(socket &&other) noexcept
    : sock_(other.sock_), pipe_notify_enabled_(other.pipe_notify_enabled_) {
  other.sock_ = NNG_SOCKET_INITIALIZER;
  other.pipe_notify_enabled_ = false;
  refresh_pipe_notifications_after_move();
}

socket &socket::operator=(socket &&other) noexcept {
  if (this != &other) {
    close();
    sock_ = other.sock_;
    pipe_notify_enabled_ = other.pipe_notify_enabled_;
    other.sock_ = NNG_SOCKET_INITIALIZER;
    other.pipe_notify_enabled_ = false;
    refresh_pipe_notifications_after_move();
  }
  return *this;
}

bool socket::valid() const { return nng_socket_id(sock_) > 0; }

nng_socket socket::get() const { return sock_; }

int socket::id() const { return nng_socket_id(sock_); }

int socket::raw(bool *raw) { return nng_socket_raw(sock_, raw); }

int socket::proto_id(std::uint16_t *idp) {
  return nng_socket_proto_id(sock_, idp);
}

int socket::peer_id(std::uint16_t *idp) {
  return nng_socket_peer_id(sock_, idp);
}

int socket::proto_name(const char **name) {
  return nng_socket_proto_name(sock_, name);
}

int socket::peer_name(const char **name) {
  return nng_socket_peer_name(sock_, name);
}

void socket::close() {
  if (!valid()) {
    return;
  }
  const auto closed_id = id();
  const auto rv = nng_socket_close(sock_);
  if (rv != 0) {
    nng2errf(cmp_socket, act_close, "id={} error={} message={}", closed_id, rv,
             nng_strerror(static_cast<::nng_err>(rv)));
  }
  pipe_notify_enabled_ = false;
  sock_ = NNG_SOCKET_INITIALIZER;
}

void socket::reset(nng_socket opened) {
  close();
  sock_ = opened;
  pipe_notify_enabled_ = false;
}

int socket::pipe_notify(bool enable) {
  if (!valid()) {
    nng2errf(cmp_pipe_notify, act_validate, "id={} invalid socket", id());
    return NNG_ECLOSED;
  }

  auto *callback = enable ? &socket::pipe_event_callback : nullptr;
  void *arg = enable ? this : nullptr;
  const auto rv = install_pipe_notifications(callback, arg);
  if (rv == 0) {
    pipe_notify_enabled_ = enable;
  }
  return rv;
}

void socket::on_pipe_add_pre(pipe &observed_pipe) {
  log_pipe_event("add-pre", observed_pipe, false);
}

void socket::on_pipe_add_post(pipe &observed_pipe) {
  log_pipe_event("add-post", observed_pipe, true);
}

void socket::on_pipe_remove_post(pipe &observed_pipe) {
  log_pipe_event("remove-post", observed_pipe, true);
}

void socket::pipe_event_callback(nng_pipe pipe, nng_pipe_ev event, void *arg) {
  auto *target = static_cast<socket *>(arg);
  if (target == nullptr) {
    return;
  }

  z::nng::pipe wrapped_pipe{pipe};
  switch (event) {
  case NNG_PIPE_EV_ADD_PRE:
    target->on_pipe_add_pre(wrapped_pipe);
    break;
  case NNG_PIPE_EV_ADD_POST:
    target->on_pipe_add_post(wrapped_pipe);
    break;
  case NNG_PIPE_EV_REM_POST:
    target->on_pipe_remove_post(wrapped_pipe);
    break;
  default:
    nng2errf(cmp_pipe_notify, act_device, "id={} unknown pipe event={} pipe={}",
             nng_socket_id(wrapped_pipe.socket()), static_cast<int>(event),
             nng_pipe_id(pipe));
    break;
  }
}

int socket::install_pipe_notifications(nng_pipe_cb callback, void *arg) {
  const auto add_pre =
      nng_pipe_notify(sock_, NNG_PIPE_EV_ADD_PRE, callback, arg);
  const auto add_post =
      nng_pipe_notify(sock_, NNG_PIPE_EV_ADD_POST, callback, arg);
  const auto remove_post =
      nng_pipe_notify(sock_, NNG_PIPE_EV_REM_POST, callback, arg);
  if (add_pre != 0 || add_post != 0 || remove_post != 0) {
    nng2errf(cmp_pipe_notify, act_start,
             "id={} failed add_pre={} add_post={} remove_post={}", id(),
             strerr(add_pre), strerr(add_post), strerr(remove_post));
    if (add_pre != 0) {
      return add_pre;
    }
    if (add_post != 0) {
      return add_post;
    }
    return remove_post;
  }
  return 0;
}

void socket::refresh_pipe_notifications_after_move() noexcept {
  if (!valid() || !pipe_notify_enabled_) {
    return;
  }
  const auto rv =
      install_pipe_notifications(&socket::pipe_event_callback, this);
  if (rv != 0) {
    pipe_notify_enabled_ = false;
  }
}

int socket::bus0_open(bool raw) {
  return open_socket(*this, raw ? "bus0-open-raw" : "bus0-open",
                     raw ? nng_bus0_open_raw : nng_bus0_open);
}

int socket::pub0_open(bool raw) {
  return open_socket(*this, raw ? "pub0-open-raw" : "pub0-open",
                     raw ? nng_pub0_open_raw : nng_pub0_open);
}

int socket::sub0_open(bool raw) {
  return open_socket(*this, raw ? "sub0-open-raw" : "sub0-open",
                     raw ? nng_sub0_open_raw : nng_sub0_open);
}

int socket::pull0_open(bool raw) {
  return open_socket(*this, raw ? "pull0-open-raw" : "pull0-open",
                     raw ? nng_pull0_open_raw : nng_pull0_open);
}

int socket::push0_open(bool raw) {
  return open_socket(*this, raw ? "push0-open-raw" : "push0-open",
                     raw ? nng_push0_open_raw : nng_push0_open);
}

int socket::rep0_open(bool raw) {
  return open_socket(*this, raw ? "rep0-open-raw" : "rep0-open",
                     raw ? nng_rep0_open_raw : nng_rep0_open);
}

int socket::req0_open(bool raw) {
  return open_socket(*this, raw ? "req0-open-raw" : "req0-open",
                     raw ? nng_req0_open_raw : nng_req0_open);
}

int socket::respondent0_open(bool raw) {
  return open_socket(*this, raw ? "respondent0-open-raw" : "respondent0-open",
                     raw ? nng_respondent0_open_raw : nng_respondent0_open);
}

int socket::surveyor0_open(bool raw) {
  return open_socket(*this, raw ? "surveyor0-open-raw" : "surveyor0-open",
                     raw ? nng_surveyor0_open_raw : nng_surveyor0_open);
}

int socket::pair0_open(bool raw) {
  return open_socket(*this, raw ? "pair0-open-raw" : "pair0-open",
                     raw ? nng_pair0_open_raw : nng_pair0_open);
}

int socket::pair1_open(bool raw) {
  return open_socket(*this, raw ? "pair1-open-raw" : "pair1-open",
                     raw ? nng_pair1_open_raw : nng_pair1_open);
}

int socket::pair1_open_poly() {
  return open_socket(*this, "pair1-poly-open", nng_pair1_open_poly);
}

int socket::set_options(const socket_options &options) {
  auto result = set_ms(NNG_OPT_SENDTIMEO, options.send_timeout_ms);
  if (result != NNG_OK) {
    return result;
  }
  result = set_ms(NNG_OPT_RECVTIMEO, options.recv_timeout_ms);
  if (result != NNG_OK) {
    return result;
  }
  result = set_int(NNG_OPT_SENDBUF, options.send_buffer);
  if (result != NNG_OK) {
    return result;
  }
  return set_int(NNG_OPT_RECVBUF, options.recv_buffer);
}

int socket::get_options(socket_options &options) const {
  socket_options result;
  auto error = get_ms(NNG_OPT_SENDTIMEO, &result.send_timeout_ms);
  if (error != NNG_OK) {
    return error;
  }
  error = get_ms(NNG_OPT_RECVTIMEO, &result.recv_timeout_ms);
  if (error != NNG_OK) {
    return error;
  }
  error = get_int(NNG_OPT_SENDBUF, &result.send_buffer);
  if (error != NNG_OK) {
    return error;
  }
  error = get_int(NNG_OPT_RECVBUF, &result.recv_buffer);
  if (error == NNG_OK) {
    options = result;
  }
  return error;
}

int socket::set_bool(const char *name, bool v) {
  const auto ret = nng_socket_set_bool(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_set_bool, "id={} nng_socket_set_bool failed: {}",
             id(), strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_set_bool, "id={} nng_socket_set_bool: {}={}", id(),
            name, v);
  }
  return ret;
}

int socket::get_bool(const char *name, bool *v) const {
  const auto ret = nng_socket_get_bool(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_get_bool, "id={} nng_socket_get_bool failed: {}",
             id(), strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_get_bool, "id={} nng_socket_get_bool: {}={}", id(),
            name, *v);
  }
  return ret;
}

int socket::set_int(const char *name, int v) {
  const auto ret = nng_socket_set_int(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_set_int, "id={} nng_socket_set_int failed: {}",
             id(), strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_set_int, "id={} nng_socket_set_int: {}={}", id(),
            name, v);
  }
  return ret;
}

int socket::get_int(const char *name, int *v) const {
  const auto ret = nng_socket_get_int(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_get_int, "id={} nng_socket_get_int failed: {}",
             id(), strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_get_int, "id={} nng_socket_get_int: {}={}", id(),
            name, *v);
  }
  return ret;
}

int socket::set_size(const char *name, std::size_t v) {
  const auto ret = nng_socket_set_size(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_set_size, "id={} nng_socket_set_size failed: {}",
             id(), strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_set_size, "id={} nng_socket_set_size: {}={}", id(),
            name, v);
  }
  return ret;
}

int socket::get_size(const char *name, std::size_t *v) const {
  const auto ret = nng_socket_get_size(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_get_size, "id={} nng_socket_get_size failed: {}",
             id(), strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_get_size, "id={} nng_socket_get_size: {}={}", id(),
            name, *v);
  }
  return ret;
}

int socket::set_ms(const char *name, nng_duration v) {
  const auto ret = nng_socket_set_ms(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_set_ms, "id={} nng_socket_set_ms failed: {}", id(),
             strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_set_ms, "id={} nng_socket_set_ms: {}={}", id(),
            name, v);
  }
  return ret;
}

int socket::get_ms(const char *name, nng_duration *v) const {
  const auto ret = nng_socket_get_ms(sock_, name, v);
  if (ret != 0) {
    nng2errf(cmp_socket, act_get_ms, "id={} nng_socket_get_ms failed: {}", id(),
             strerr(ret));
  } else {
    nng2dbg(cmp_socket, act_get_ms, "id={} nng_socket_get_ms: {}={}", id(),
            name, *v);
  }
  return ret;
}

z::err_t socket::listen(const endpoint &target) {
  if (!valid()) {
    nng2errf(cmp_socket, act_listen, "id={} invalid socket", id());
    return z::ERR_FAIL;
  }
  std::string error;
  if (remove_stale_ipc_socket(target.url, &error) != z::ERR_OK) {
    nng2errf(cmp_socket, act_listen, "id={} {}", id(), error);
    return z::ERR_FAIL;
  }
  const auto rv = nng_listen(sock_, target.url.c_str(), nullptr, 0);
  if (rv != 0) {
    nng2errf(cmp_socket, act_listen, "id={} error={} message={} url={}", id(),
             rv, nng_strerror(static_cast<::nng_err>(rv)), target.url);
    return z::ERR_FAIL;
  }
  nng2inf(cmp_socket, act_listen, "id={} transport={} url={}", id(),
          transport_name(target.transport), target.url);
  return z::ERR_OK;
}

z::err_t socket::dial(const endpoint &target, bool nonblock) {
  if (!valid()) {
    nng2errf(cmp_socket, act_dial, "id={} invalid socket", id());
    return z::ERR_FAIL;
  }
  const int flags = nonblock ? NNG_FLAG_NONBLOCK : 0;
  const auto rv = nng_dial(sock_, target.url.c_str(), nullptr, flags);
  if (rv != 0) {
    nng2errf(cmp_socket, act_dial, "id={} error={} message={} url={}", id(), rv,
             nng_strerror(static_cast<::nng_err>(rv)), target.url);
    return z::ERR_FAIL;
  }
  nng2inf(cmp_socket, act_dial, "id={} transport={} url={}", id(),
          transport_name(target.transport), target.url);
  return z::ERR_OK;
}

} // namespace z::nng
