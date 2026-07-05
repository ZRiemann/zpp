#include <zpp/nng/url.h>

#include <cstddef>
#include <utility>

namespace z::nng {

url::url(nng_url *value) noexcept : url_(value) {}

url::~url() noexcept { reset(); }

url::url(url &&other) noexcept : url_(other.release()) {}

url &url::operator=(url &&other) noexcept {
  if (this != &other) {
    reset(other.release());
  }
  return *this;
}

int url::parse(const char *text, url &out) noexcept {
  if (text == nullptr) {
    return NNG_EINVAL;
  }

  nng_url *parsed{nullptr};
  const auto rv = nng_url_parse(&parsed, text);
  if (rv == 0) {
    out.reset(parsed);
  }
  return rv;
}

int url::clone(const url &source, url &out) noexcept {
  if (!source.valid()) {
    return NNG_EINVAL;
  }

  nng_url *cloned{nullptr};
  const auto rv = nng_url_clone(&cloned, source.get());
  if (rv == 0) {
    out.reset(cloned);
  }
  return rv;
}

int url::parse(const char *text) noexcept { return parse(text, *this); }

int url::clone(url &out) const noexcept { return clone(*this, out); }

bool url::valid() const noexcept { return url_ != nullptr; }

nng_url *url::get() const noexcept { return url_; }

nng_url *url::release() noexcept {
  auto *released = url_;
  url_ = nullptr;
  return released;
}

void url::reset(nng_url *value) noexcept {
  if (url_ == value) {
    return;
  }
  if (url_ != nullptr) {
    nng_url_free(url_);
  }
  url_ = value;
}

void url::swap(url &other) noexcept { std::swap(url_, other.url_); }

const char *url::scheme() const noexcept {
  return valid() ? nng_url_scheme(url_) : nullptr;
}

const char *url::userinfo() const noexcept {
  return valid() ? nng_url_userinfo(url_) : nullptr;
}

const char *url::hostname() const noexcept {
  return valid() ? nng_url_hostname(url_) : nullptr;
}

std::uint32_t url::port() const noexcept {
  return valid() ? nng_url_port(url_) : 0U;
}

const char *url::path() const noexcept {
  return valid() ? nng_url_path(url_) : nullptr;
}

const char *url::query() const noexcept {
  return valid() ? nng_url_query(url_) : nullptr;
}

const char *url::fragment() const noexcept {
  return valid() ? nng_url_fragment(url_) : nullptr;
}

void url::resolve_port(std::uint32_t port) noexcept {
  if (valid()) {
    nng_url_resolve_port(url_, port);
  }
}

std::string url::str() const {
  if (!valid()) {
    return {};
  }

  const auto len = nng_url_sprintf(nullptr, 0, url_);
  if (len < 0) {
    return {};
  }

  std::string result(static_cast<std::size_t>(len) + 1U, '\0');
  const auto written = nng_url_sprintf(result.data(), result.size(), url_);
  if (written < 0) {
    return {};
  }
  result.resize(static_cast<std::size_t>(written));
  return result;
}

void swap(url &lhs, url &rhs) noexcept { lhs.swap(rhs); }

} // namespace z::nng
