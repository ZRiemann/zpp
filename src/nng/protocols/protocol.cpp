#include <zpp/nng/protocols/protocol.h>

#include <algorithm>
#include <string>
#include <thread>
#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {
namespace {

constexpr std::size_t max_concurrency{128};

z::err_t create_listener(socket &owner, const endpoint &target_endpoint,
                         const transport_options *transport_config,
                         const listener_options *listener_config,
                         std::vector<listener> &listeners) {
  if (target_endpoint.transport == transport::ipc) {
    std::string error;
    if (remove_stale_ipc_socket(target_endpoint.url, &error) != z::ERR_OK) {
      nng2errf(cmp_listener, act_create, "{}", error);
      return z::ERR_FAIL;
    }
  }

  listener created;
  if (created.create(owner.get(), target_endpoint.url) != NNG_OK) {
    return z::ERR_FAIL;
  }
  const auto *tcp_config =
      target_endpoint.transport == transport::tcp ? transport_config : nullptr;
  if (created.set_options(listener_config, tcp_config) != NNG_OK) {
    return z::ERR_FAIL;
  }
  listeners.push_back(std::move(created));
  return z::ERR_OK;
}

z::err_t create_dialer(socket &owner, const endpoint &target_endpoint,
                       const transport_options *transport_config,
                       const dialer_options *dialer_config,
                       std::vector<dialer> &dialers) {
  dialer created;
  if (created.create(owner.get(), target_endpoint.url) != NNG_OK) {
    return z::ERR_FAIL;
  }
  const auto *tcp_config =
      target_endpoint.transport == transport::tcp ? transport_config : nullptr;
  if (created.set_options(dialer_config, tcp_config) != NNG_OK) {
    return z::ERR_FAIL;
  }
  dialers.push_back(std::move(created));
  return z::ERR_OK;
}

} // namespace

endpoint_protocol::endpoint_protocol(std::string_view component) noexcept
    : component_(component) {}

z::err_t endpoint_protocol::configure(open_fn open_socket,
                                      std::vector<endpoint> &&endpoints,
                                      const socket_options *socket_config,
                                      const transport_options *transport_config,
                                      const dialer_options *dialer_config,
                                      const listener_options *listener_config,
                                      configure_fn protocol_configure,
                                      const void *protocol_config) {
  stop();
  if (validate_endpoints(endpoints) != z::ERR_OK) {
    nng2errf(component_, act_configure,
             "stage=validate-endpoints endpoint-count={}", endpoints.size());
    stop();
    return z::ERR_FAIL;
  }

  int result = (socket_.*open_socket)(false);
  if (result != NNG_OK) {
    nng2errf(component_, act_configure, "stage=open-socket error={} message={}",
             result, nng_strerror(static_cast<nng_err>(result)));
    stop();
    return z::ERR_FAIL;
  }

  if (socket_config != nullptr) {
    result = socket_.set_options(*socket_config);
    if (result != NNG_OK) {
      nng2errf(component_, act_configure,
               "stage=socket-options error={} message={}", result,
               nng_strerror(static_cast<nng_err>(result)));
      stop();
      return z::ERR_FAIL;
    }
  }

  if (protocol_configure != nullptr) {
    result = protocol_configure(socket_, protocol_config);
    if (result != NNG_OK) {
      nng2errf(component_, act_configure,
               "stage=protocol-options error={} message={}", result,
               nng_strerror(static_cast<nng_err>(result)));
      stop();
      return z::ERR_FAIL;
    }
  }

  result = socket_.pipe_notify(true);
  if (result != NNG_OK) {
    nng2errf(component_, act_configure, "stage=pipe-notify error={} message={}",
             result, nng_strerror(static_cast<nng_err>(result)));
    stop();
    return z::ERR_FAIL;
  }

  if (configure_endpoints(endpoints, transport_config, dialer_config,
                          listener_config) != z::ERR_OK) {
    nng2errf(component_, act_configure,
             "stage=configure-endpoints endpoint-count={}", endpoints.size());
    stop();
    return z::ERR_FAIL;
  }

  endpoints_ = std::move(endpoints);
  return z::ERR_OK;
}

z::err_t endpoint_protocol::start() {
  if (!socket_.valid()) {
    nng2errf(component_, act_start, "invalid socket; configure() must succeed");
    stop();
    return z::ERR_FAIL;
  }
  if (endpoints_.empty()) {
    nng2errf(component_, act_start,
             "no configured endpoints; configure() must succeed");
    stop();
    return z::ERR_FAIL;
  }
  if (start_endpoints() != z::ERR_OK) {
    nng2errf(component_, act_start,
             "endpoint startup failed listeners={} dialers={}",
             listeners_.size(), dialers_.size());
    stop();
    return z::ERR_FAIL;
  }
  return z::ERR_OK;
}

void endpoint_protocol::stop() {
  dialers_.clear();
  listeners_.clear();
  socket_.close();
  endpoints_.clear();
}

const std::vector<endpoint> &endpoint_protocol::endpoints() const noexcept {
  return endpoints_;
}

int endpoint_protocol::open_context(aio_ctx &context,
                                    aio_ctx::callback callback,
                                    void *arg) const noexcept {
  if (!socket_.valid()) {
    nng2errf(component_, act_create,
             "failed to open context: protocol socket is closed");
    return NNG_ECLOSED;
  }
  const int result = context.open(socket_.get(), callback, arg);
  if (result != NNG_OK) {
    nng2errf(component_, act_create,
             "failed to open context error={} message={}", result,
             nng_strerror(static_cast<nng_err>(result)));
  }
  return result;
}

int endpoint_protocol::send(message &msg, int flags) noexcept {
  return socket_.send(msg, flags);
}

void endpoint_protocol::send(aio &io) noexcept { socket_.send(io); }

int endpoint_protocol::recv(message &msg, int flags) noexcept {
  return socket_.recv(msg, flags);
}

void endpoint_protocol::recv(aio &io) noexcept { socket_.recv(io); }

std::size_t
endpoint_protocol::normalize_concurrency(std::size_t requested) noexcept {
  if (requested == 0) {
    return static_cast<std::size_t>(std::thread::hardware_concurrency()) + 1U;
  }
  return std::min(requested, max_concurrency);
}

socket &endpoint_protocol::protocol_socket() noexcept { return socket_; }

const socket &endpoint_protocol::protocol_socket() const noexcept {
  return socket_;
}

z::err_t endpoint_protocol::configure_endpoints(
    const std::vector<endpoint> &endpoints,
    const transport_options *transport_config,
    const dialer_options *dialer_config,
    const listener_options *listener_config) {
  const auto listener_count = std::count_if(
      endpoints.begin(), endpoints.end(),
      [](const endpoint &item) { return item.role == endpoint_role::listen; });
  listeners_.reserve(static_cast<std::size_t>(listener_count));
  dialers_.reserve(endpoints.size() - static_cast<std::size_t>(listener_count));

  for (const auto &target_endpoint : endpoints) {
    const auto result =
        target_endpoint.role == endpoint_role::listen
            ? create_listener(socket_, target_endpoint, transport_config,
                              listener_config, listeners_)
            : create_dialer(socket_, target_endpoint, transport_config,
                            dialer_config, dialers_);
    if (result != z::ERR_OK) {
      nng2errf(component_, act_configure, "failed endpoint role={} url={}",
               endpoint_role_name(target_endpoint.role), target_endpoint.url);
      return z::ERR_FAIL;
    }
  }
  return z::ERR_OK;
}

z::err_t endpoint_protocol::start_endpoints() {
  for (auto &endpoint_listener : listeners_) {
    const int result = endpoint_listener.start();
    if (result != NNG_OK) {
      nng2errf(component_, act_start,
               "failed to start listener id={} error={} message={}",
               endpoint_listener.id(), result,
               nng_strerror(static_cast<nng_err>(result)));
      return z::ERR_FAIL;
    }
  }
  for (auto &endpoint_dialer : dialers_) {
    const int result = endpoint_dialer.start(NNG_FLAG_NONBLOCK);
    if (result != NNG_OK) {
      nng2errf(component_, act_start,
               "failed to start dialer id={} error={} message={}",
               endpoint_dialer.id(), result,
               nng_strerror(static_cast<nng_err>(result)));
      return z::ERR_FAIL;
    }
  }
  return z::ERR_OK;
}

} // namespace z::nng
