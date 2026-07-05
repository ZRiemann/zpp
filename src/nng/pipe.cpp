#include <zpp/nng/pipe.h>

#include <zpp/nng/log.h>

namespace z::nng {

pipe::pipe(nng_pipe p) noexcept : pipe_(p) {}

nng_pipe pipe::get() const noexcept { return pipe_; }

void pipe::reset(nng_pipe p) noexcept { pipe_ = p; }

bool pipe::valid() const noexcept { return id() > 0; }

int pipe::id() const noexcept { return nng_pipe_id(pipe_); }

int pipe::close() const noexcept { return nng_pipe_close(pipe_); }

nng_dialer pipe::dialer() const noexcept { return nng_pipe_dialer(pipe_); }

nng_listener pipe::listener() const noexcept {
  return nng_pipe_listener(pipe_);
}

nng_socket pipe::socket() const noexcept { return nng_pipe_socket(pipe_); }

int pipe::notify(nng_pipe_ev ev, nng_pipe_cb cb, void *arg) const noexcept {
  return nng_pipe_notify(socket(), ev, cb, arg);
}

int pipe::peer_addr(nng_sockaddr *addr) const noexcept {
  return nng_pipe_peer_addr(pipe_, addr);
}

int pipe::peer_addr(nng_sockaddr &addr) const noexcept {
  return peer_addr(&addr);
}

int pipe::self_addr(nng_sockaddr *addr) const noexcept {
  return nng_pipe_self_addr(pipe_, addr);
}

int pipe::self_addr(nng_sockaddr &addr) const noexcept {
  return self_addr(&addr);
}

int pipe::get_bool(const char *opt, bool &val) const noexcept {
  const auto rv = nng_pipe_get_bool(pipe_, opt, &val);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_bool, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val);
  }
  return rv;
}

int pipe::get_int(const char *opt, int &val) const noexcept {
  const auto rv = nng_pipe_get_int(pipe_, opt, &val);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_int, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val);
  }
  return rv;
}

int pipe::get_ms(const char *opt, nng_duration &val) const noexcept {
  const auto rv = nng_pipe_get_ms(pipe_, opt, &val);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_ms, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val);
  }
  return rv;
}

int pipe::get_size(const char *opt, std::size_t &val) const noexcept {
  const auto rv = nng_pipe_get_size(pipe_, opt, &val);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_size, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val);
  }
  return rv;
}

int pipe::get_string(const char *opt, const char *&val) const noexcept {
  const auto rv = nng_pipe_get_string(pipe_, opt, &val);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_string, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val == nullptr ? "<null>" : val);
  }
  return rv;
}

int pipe::get_strcpy(const char *opt, char *val,
                     std::size_t len) const noexcept {
  const auto rv = nng_pipe_get_strcpy(pipe_, opt, val, len);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_string, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val == nullptr ? "<null>" : val);
  }
  return rv;
}

int pipe::get_strdup(const char *opt, char *&val) const noexcept {
  const auto rv = nng_pipe_get_strdup(pipe_, opt, &val);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_string, "option={} value={}",
            opt == nullptr ? "<null>" : opt, val == nullptr ? "<null>" : val);
  }
  return rv;
}

int pipe::get_strlen(const char *opt, std::size_t &len) const noexcept {
  const auto rv = nng_pipe_get_strlen(pipe_, opt, &len);
  if (rv == 0) {
    nng2dbg(cmp_pipe, act_get_size, "option={} value={}",
            opt == nullptr ? "<null>" : opt, len);
  }
  return rv;
}

std::string pipe::get_address(bool peer) const {
  nng_sockaddr address{};
  const auto rv = peer ? peer_addr(address) : self_addr(address);
  if (rv != 0) {
    return std::string{"<"} + nng_strerror(static_cast<::nng_err>(rv)) + ">";
  }

  char buffer[NNG_MAXADDRSTRLEN]{};
  const auto *text = nng_str_sockaddr(&address, buffer, sizeof(buffer));
  return text == nullptr ? std::string{"<unknown>"} : std::string{text};
}

} // namespace z::nng
