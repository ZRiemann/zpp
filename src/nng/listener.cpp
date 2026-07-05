#include <zpp/nng/listener.h>

#include <string>

#include <zpp/nng/log.h>
#include <zpp/nng/url.h>

namespace z::nng {

listener::listener(nng_listener handle) noexcept : listener_(handle) {}

listener::~listener() noexcept { close(); }

listener::listener(listener &&other) noexcept : listener_(other.release()) {}

listener &listener::operator=(listener &&other) noexcept {
  if (this != &other) {
    reset(other.release());
  }
  return *this;
}

bool listener::valid() const noexcept { return id() > 0; }

listener::operator bool() const noexcept { return valid(); }

int listener::id() const noexcept { return nng_listener_id(listener_); }

nng_listener listener::get() const noexcept { return listener_; }

nng_listener listener::release() noexcept {
  const auto released = listener_;
  listener_ = NNG_LISTENER_INITIALIZER;
  return released;
}

void listener::reset(nng_listener handle) noexcept {
  if (listener_.id == handle.id) {
    return;
  }
  close();
  listener_ = handle;
}

void listener::close() noexcept {
  if (!valid()) {
    return;
  }
  const auto rv = nng_listener_close(listener_);
  if (rv != 0) {
    nng2err(cmp_listener, act_close, rv);
  }
  listener_ = NNG_LISTENER_INITIALIZER;
}

int listener::create(nng_socket socket, const char *target_url) noexcept {
  if (target_url == nullptr || target_url[0] == '\0') {
    nng2err_url(cmp_listener, act_create, NNG_EINVAL, target_url);
    return NNG_EINVAL;
  }

  nng_listener handle{NNG_LISTENER_INITIALIZER};
  const auto rv = nng_listener_create(&handle, socket, target_url);
  if (rv != 0) {
    nng2err_url(cmp_listener, act_create, rv, target_url);
    return rv;
  }
  reset(handle);
  nng2inf(cmp_listener, act_create, "id={} url={}", id(), target_url);
  return rv;
}

int listener::create(nng_socket socket, std::string_view target_url) {
  const std::string owned_url{target_url};
  return create(socket, owned_url.c_str());
}

int listener::create(nng_socket socket, const nng_url *target_url) noexcept {
  if (target_url == nullptr) {
    nng2err_url(cmp_listener, act_create, NNG_EINVAL, "<null-url>");
    return NNG_EINVAL;
  }

  nng_listener handle{NNG_LISTENER_INITIALIZER};
  const auto rv = nng_listener_create_url(&handle, socket, target_url);
  if (rv != 0) {
    nng2err_url(cmp_listener, act_create, rv, "<nng-url>");
    return rv;
  }
  reset(handle);
  nng2inf(cmp_listener, act_create, "id={} url=<nng-url>", id());
  return rv;
}

int listener::create(nng_socket socket, const url &target_url) noexcept {
  return create(socket, target_url.get());
}

int listener::start(int flags) noexcept {
  const auto rv = nng_listener_start(listener_, flags);
  if (rv != 0) {
    nng2err(cmp_listener, act_start, rv);
    return rv;
  }
  nng2inf(cmp_listener, act_start, "id={} flags={}", id(), flags);
  return rv;
}

int listener::set_options(const listener_options *options,
                          const transport_options *transport) noexcept {
  if (options != nullptr) {
    const auto result =
        set_size(NNG_OPT_RECVMAXSZ, options->max_recv_size_bytes);
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

int listener::get_options(listener_options *options,
                          transport_options *transport) const noexcept {
  listener_options listener_result;
  transport_options transport_result;
  if (options != nullptr) {
    const auto result =
        get_size(NNG_OPT_RECVMAXSZ, &listener_result.max_recv_size_bytes);
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
    *options = listener_result;
  }
  if (transport != nullptr) {
    *transport = transport_result;
  }
  return NNG_OK;
}

int listener::set_bool(const char *name, bool value) noexcept {
  const auto rv = nng_listener_set_bool(listener_, name, value);
  nng2opt_err(cmp_listener, act_set_bool, rv, name);
  return rv;
}

int listener::get_bool(const char *name, bool *value) const noexcept {
  const auto rv = nng_listener_get_bool(listener_, name, value);
  nng2opt_err(cmp_listener, act_get_bool, rv, name);
  return rv;
}

int listener::set_int(const char *name, int value) noexcept {
  const auto rv = nng_listener_set_int(listener_, name, value);
  nng2opt_err(cmp_listener, act_set_int, rv, name);
  return rv;
}

int listener::get_int(const char *name, int *value) const noexcept {
  const auto rv = nng_listener_get_int(listener_, name, value);
  nng2opt_err(cmp_listener, act_get_int, rv, name);
  return rv;
}

int listener::set_size(const char *name, std::size_t value) noexcept {
  const auto rv = nng_listener_set_size(listener_, name, value);
  nng2opt_err(cmp_listener, act_set_size, rv, name);
  return rv;
}

int listener::get_size(const char *name, std::size_t *value) const noexcept {
  const auto rv = nng_listener_get_size(listener_, name, value);
  nng2opt_err(cmp_listener, act_get_size, rv, name);
  return rv;
}

int listener::set_string(const char *name, const char *value) noexcept {
  const auto rv = nng_listener_set_string(listener_, name, value);
  nng2opt_err(cmp_listener, act_set_string, rv, name);
  return rv;
}

int listener::get_string(const char *name, const char **value) const noexcept {
  const auto rv = nng_listener_get_string(listener_, name, value);
  nng2opt_err(cmp_listener, act_get_string, rv, name);
  return rv;
}

int listener::set_ms(const char *name, nng_duration value) noexcept {
  const auto rv = nng_listener_set_ms(listener_, name, value);
  nng2opt_err(cmp_listener, act_set_ms, rv, name);
  return rv;
}

int listener::get_ms(const char *name, nng_duration *value) const noexcept {
  const auto rv = nng_listener_get_ms(listener_, name, value);
  nng2opt_err(cmp_listener, act_get_ms, rv, name);
  return rv;
}

int listener::set_tls(nng_tls_config *value) noexcept {
  const auto rv = nng_listener_set_tls(listener_, value);
  nng2err(cmp_listener, act_set_tls, rv);
  return rv;
}

int listener::get_tls(nng_tls_config **value) const noexcept {
  const auto rv = nng_listener_get_tls(listener_, value);
  nng2err(cmp_listener, act_get_tls, rv);
  return rv;
}

int listener::set_security_descriptor(void *value) noexcept {
  const auto rv = nng_listener_set_security_descriptor(listener_, value);
  nng2err(cmp_listener, act_set_security_descriptor, rv);
  return rv;
}

int listener::get_url(const nng_url **value) const noexcept {
  const auto rv = nng_listener_get_url(listener_, value);
  nng2err(cmp_listener, act_get_url, rv);
  return rv;
}

} // namespace z::nng
