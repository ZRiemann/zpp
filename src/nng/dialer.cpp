#include <zpp/nng/dialer.h>

#include <string>

#include <zpp/nng/aio.h>
#include <zpp/nng/log.h>
#include <zpp/nng/url.h>

namespace z::nng {

dialer::dialer(nng_dialer handle) noexcept : dialer_(handle) {}

dialer::~dialer() noexcept { close(); }

dialer::dialer(dialer &&other) noexcept : dialer_(other.release()) {}

dialer &dialer::operator=(dialer &&other) noexcept {
  if (this != &other) {
    reset(other.release());
  }
  return *this;
}

bool dialer::valid() const noexcept { return id() > 0; }

dialer::operator bool() const noexcept { return valid(); }

int dialer::id() const noexcept { return nng_dialer_id(dialer_); }

nng_dialer dialer::get() const noexcept { return dialer_; }

nng_dialer dialer::release() noexcept {
  const auto released = dialer_;
  dialer_ = NNG_DIALER_INITIALIZER;
  return released;
}

void dialer::reset(nng_dialer handle) noexcept {
  if (dialer_.id == handle.id) {
    return;
  }
  close();
  dialer_ = handle;
}

void dialer::close() noexcept {
  if (!valid()) {
    return;
  }
  const auto rv = nng_dialer_close(dialer_);
  if (rv != 0) {
    nng2err(cmp_dialer, act_close, rv);
  }
  dialer_ = NNG_DIALER_INITIALIZER;
}

int dialer::create(nng_socket socket, const char *target_url) noexcept {
  if (target_url == nullptr || target_url[0] == '\0') {
    nng2err_url(cmp_dialer, act_create, NNG_EINVAL, target_url);
    return NNG_EINVAL;
  }

  nng_dialer handle{NNG_DIALER_INITIALIZER};
  const auto rv = nng_dialer_create(&handle, socket, target_url);
  if (rv != 0) {
    nng2err_url(cmp_dialer, act_create, rv, target_url);
    return rv;
  }
  reset(handle);
  nng2inf(cmp_dialer, act_create, "id={} url={}", id(), target_url);
  return rv;
}

int dialer::create(nng_socket socket, std::string_view target_url) {
  const std::string owned_url{target_url};
  return create(socket, owned_url.c_str());
}

int dialer::create(nng_socket socket, const nng_url *target_url) noexcept {
  if (target_url == nullptr) {
    nng2err_url(cmp_dialer, act_create, NNG_EINVAL, "<null-url>");
    return NNG_EINVAL;
  }

  nng_dialer handle{NNG_DIALER_INITIALIZER};
  const auto rv = nng_dialer_create_url(&handle, socket, target_url);
  if (rv != 0) {
    nng2err_url(cmp_dialer, act_create, rv, "<nng-url>");
    return rv;
  }
  reset(handle);
  nng2inf(cmp_dialer, act_create, "id={} url=<nng-url>", id());
  return rv;
}

int dialer::create(nng_socket socket, const url &target_url) noexcept {
  return create(socket, target_url.get());
}

int dialer::start(int flags) noexcept {
  const auto rv = nng_dialer_start(dialer_, flags);
  if (rv != 0) {
    nng2err(cmp_dialer, act_start, rv);
    return rv;
  }
  nng2inf(cmp_dialer, act_start, "id={} flags={}", id(), flags);
  return rv;
}

void dialer::start(aio &io, int flags) noexcept {
  if (!valid()) {
    nng2err(cmp_dialer, act_start_aio, NNG_ECLOSED);
    return;
  }
  nng_dialer_start_aio(dialer_, flags, io.get());
  nng2dbg(cmp_dialer, act_start_aio, "id={} flags={}", id(), flags);
}

int dialer::set_options(const dialer_options *options,
                        const transport_options *transport) noexcept {
  if (options != nullptr) {
    auto result = set_size(NNG_OPT_RECVMAXSZ, options->max_recv_size_bytes);
    if (result != NNG_OK) {
      return result;
    }
    result = set_ms(NNG_OPT_RECONNMINT, options->reconnect_min_ms);
    if (result != NNG_OK) {
      return result;
    }
    result = set_ms(NNG_OPT_RECONNMAXT, options->reconnect_max_ms);
    if (result != NNG_OK) {
      return result;
    }
  }
  if (transport != nullptr) {
    auto result = set_bool(NNG_OPT_TCP_NODELAY, transport->tcp_no_delay);
    if (result != NNG_OK) {
      return result;
    }
    result = set_bool(NNG_OPT_TCP_KEEPALIVE, transport->tcp_keepalive);
    if (result != NNG_OK) {
      return result;
    }
  }
  return NNG_OK;
}

int dialer::get_options(dialer_options *options,
                        transport_options *transport) const noexcept {
  dialer_options dialer_result;
  transport_options transport_result;
  if (options != nullptr) {
    auto result =
        get_size(NNG_OPT_RECVMAXSZ, &dialer_result.max_recv_size_bytes);
    if (result != NNG_OK) {
      return result;
    }
    result = get_ms(NNG_OPT_RECONNMINT, &dialer_result.reconnect_min_ms);
    if (result != NNG_OK) {
      return result;
    }
    result = get_ms(NNG_OPT_RECONNMAXT, &dialer_result.reconnect_max_ms);
    if (result != NNG_OK) {
      return result;
    }
  }
  if (transport != nullptr) {
    auto result = get_bool(NNG_OPT_TCP_NODELAY, &transport_result.tcp_no_delay);
    if (result != NNG_OK) {
      return result;
    }
    result = get_bool(NNG_OPT_TCP_KEEPALIVE, &transport_result.tcp_keepalive);
    if (result != NNG_OK) {
      return result;
    }
  }
  if (options != nullptr) {
    *options = dialer_result;
  }
  if (transport != nullptr) {
    *transport = transport_result;
  }
  return NNG_OK;
}

int dialer::set_bool(const char *name, bool value) noexcept {
  const auto rv = nng_dialer_set_bool(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_set_bool, rv, name);
  return rv;
}

int dialer::get_bool(const char *name, bool *value) const noexcept {
  const auto rv = nng_dialer_get_bool(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_get_bool, rv, name);
  return rv;
}

int dialer::set_int(const char *name, int value) noexcept {
  const auto rv = nng_dialer_set_int(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_set_int, rv, name);
  return rv;
}

int dialer::get_int(const char *name, int *value) const noexcept {
  const auto rv = nng_dialer_get_int(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_get_int, rv, name);
  return rv;
}

int dialer::set_size(const char *name, std::size_t value) noexcept {
  const auto rv = nng_dialer_set_size(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_set_size, rv, name);
  return rv;
}

int dialer::get_size(const char *name, std::size_t *value) const noexcept {
  const auto rv = nng_dialer_get_size(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_get_size, rv, name);
  return rv;
}

int dialer::set_string(const char *name, const char *value) noexcept {
  const auto rv = nng_dialer_set_string(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_set_string, rv, name);
  return rv;
}

int dialer::get_string(const char *name, const char **value) const noexcept {
  const auto rv = nng_dialer_get_string(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_get_string, rv, name);
  return rv;
}

int dialer::set_ms(const char *name, nng_duration value) noexcept {
  const auto rv = nng_dialer_set_ms(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_set_ms, rv, name);
  return rv;
}

int dialer::get_ms(const char *name, nng_duration *value) const noexcept {
  const auto rv = nng_dialer_get_ms(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_get_ms, rv, name);
  return rv;
}

int dialer::set_addr(const char *name, const nng_sockaddr *value) noexcept {
  const auto rv = nng_dialer_set_addr(dialer_, name, value);
  nng2opt_err(cmp_dialer, act_set_addr, rv, name);
  return rv;
}

int dialer::set_tls(nng_tls_config *value) noexcept {
  const auto rv = nng_dialer_set_tls(dialer_, value);
  nng2err(cmp_dialer, act_set_tls, rv);
  return rv;
}

int dialer::get_tls(nng_tls_config **value) const noexcept {
  const auto rv = nng_dialer_get_tls(dialer_, value);
  nng2err(cmp_dialer, act_get_tls, rv);
  return rv;
}

int dialer::get_url(const nng_url **value) const noexcept {
  const auto rv = nng_dialer_get_url(dialer_, value);
  nng2err(cmp_dialer, act_get_url, rv);
  return rv;
}

} // namespace z::nng
