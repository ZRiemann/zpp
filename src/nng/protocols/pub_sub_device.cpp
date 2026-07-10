#include <zpp/nng/protocols/pub_sub_device.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {
namespace {

std::size_t count_role(const std::vector<endpoint> &endpoints,
                       endpoint_role role) {
  return static_cast<std::size_t>(
      std::count_if(endpoints.begin(), endpoints.end(),
                    [role](const endpoint &item) {
                      return item.role == role;
                    }));
}

z::err_t create_dialer(socket &owner, const endpoint &target,
                       const transport_options *transport_config,
                       const dialer_options *dialer_config,
                       std::vector<dialer> &dialers) {
  dialer created;
  if (created.create(owner.get(), target.url) != NNG_OK) {
    return z::ERR_FAIL;
  }
  const auto *tcp_config =
      target.transport == transport::tcp ? transport_config : nullptr;
  if (created.set_options(dialer_config, tcp_config) != NNG_OK) {
    return z::ERR_FAIL;
  }
  dialers.push_back(std::move(created));
  return z::ERR_OK;
}

z::err_t create_listener(socket &owner, const endpoint &target,
                         const transport_options *transport_config,
                         const listener_options *listener_config,
                         std::vector<listener> &listeners) {
  if (target.transport == transport::ipc) {
    std::string error;
    if (remove_stale_ipc_socket(target.url, &error) != z::ERR_OK) {
      nng2errf(cmp_listener, act_create, "{}", error);
      return z::ERR_FAIL;
    }
  }

  listener created;
  if (created.create(owner.get(), target.url) != NNG_OK) {
    return z::ERR_FAIL;
  }
  const auto *tcp_config =
      target.transport == transport::tcp ? transport_config : nullptr;
  if (created.set_options(listener_config, tcp_config) != NNG_OK) {
    return z::ERR_FAIL;
  }
  listeners.push_back(std::move(created));
  return z::ERR_OK;
}

z::err_t configure_device_endpoints(
    socket &owner, const std::vector<endpoint> &endpoints,
    const transport_options *transport_config,
    const dialer_options *dialer_config,
    const listener_options *listener_config, std::vector<listener> &listeners,
    std::vector<dialer> &dialers) {
  listeners.reserve(count_role(endpoints, endpoint_role::listen));
  dialers.reserve(count_role(endpoints, endpoint_role::dial));
  for (const auto &target : endpoints) {
    const auto result =
        target.role == endpoint_role::listen
            ? create_listener(owner, target, transport_config, listener_config,
                              listeners)
            : create_dialer(owner, target, transport_config, dialer_config,
                            dialers);
    if (result != z::ERR_OK) {
      nng2errf(cmp_pub_sub_device, act_configure,
               "failed endpoint role={} url={}",
               endpoint_role_name(target.role), target.url);
      return z::ERR_FAIL;
    }
  }
  return z::ERR_OK;
}

z::err_t start_listeners(std::string_view device_name,
                         std::vector<listener> &listeners) {
  for (auto &endpoint_listener : listeners) {
    const int result = endpoint_listener.start();
    if (result != NNG_OK) {
      nng2errf(cmp_pub_sub_device, act_start,
               "name={} failed to start listener id={} error={} message={}",
               device_name, endpoint_listener.id(), result,
               nng_strerror(static_cast<nng_err>(result)));
      return z::ERR_FAIL;
    }
  }
  return z::ERR_OK;
}

z::err_t start_dialers(std::string_view device_name,
                       std::vector<dialer> &dialers) {
  for (auto &endpoint_dialer : dialers) {
    const int result = endpoint_dialer.start(NNG_FLAG_NONBLOCK);
    if (result != NNG_OK) {
      nng2errf(cmp_pub_sub_device, act_start,
               "name={} failed to start dialer id={} error={} message={}",
               device_name, endpoint_dialer.id(), result,
               nng_strerror(static_cast<nng_err>(result)));
      return z::ERR_FAIL;
    }
  }
  return z::ERR_OK;
}

} // namespace

pub_sub_device::~pub_sub_device() { stop(); }

z::err_t pub_sub_device::configure(std::string name,
                                   std::vector<endpoint> &&ingress,
                                   std::vector<endpoint> &&egress,
                                   const socket_options *socket_config,
                                   const transport_options *transport_config,
                                   const dialer_options *dialer_config,
                                   const listener_options *listener_config) {
  stop();
  name_ = std::move(name);
  if (ingress.empty() || egress.empty() ||
      validate_endpoints(ingress) != z::ERR_OK ||
      validate_endpoints(egress) != z::ERR_OK) {
    nng2errf(cmp_pub_sub_device, act_configure,
             "name={} invalid endpoint configuration",
             name_);
    return z::ERR_FAIL;
  }

  auto rv = front_.sub0_open(true);
  if (rv != 0) {
    return z::ERR_FAIL;
  }

  rv = back_.pub0_open(true);
  if (rv != 0) {
    stop();
    return z::ERR_FAIL;
  }

  if (socket_config != nullptr) {
    if (front_.set_options(*socket_config) != NNG_OK ||
        back_.set_options(*socket_config) != NNG_OK) {
      stop();
      return z::ERR_FAIL;
    }
  }

  if (front_.pipe_notify(true) != NNG_OK || back_.pipe_notify(true) != NNG_OK) {
    stop();
    return z::ERR_FAIL;
  }

  if (configure_device_endpoints(front_, ingress, transport_config,
                                 dialer_config, listener_config,
                                 ingress_listeners_,
                                 ingress_dialers_) != z::ERR_OK ||
      configure_device_endpoints(back_, egress, transport_config, dialer_config,
                                 listener_config, egress_listeners_,
                                 egress_dialers_) != z::ERR_OK) {
    stop();
    return z::ERR_FAIL;
  }

  ingress_endpoints_ = std::move(ingress);
  egress_endpoints_ = std::move(egress);
  return z::ERR_OK;
}

z::err_t pub_sub_device::start() {
  if (!front_.valid() || !back_.valid() || ingress_endpoints_.empty() ||
      egress_endpoints_.empty()) {
    nng2errf(cmp_pub_sub_device, act_start,
             "name={} invalid device; configure() must succeed", name_);
    stop();
    return z::ERR_FAIL;
  }

  if (start_listeners(name_, ingress_listeners_) != z::ERR_OK ||
      start_listeners(name_, egress_listeners_) != z::ERR_OK ||
      start_dialers(name_, ingress_dialers_) != z::ERR_OK ||
      start_dialers(name_, egress_dialers_) != z::ERR_OK) {
    stop();
    return z::ERR_FAIL;
  }

  const int rv = aio_.create(&pub_sub_device::on_event, this);
  if (rv != NNG_OK) {
    stop();
    return z::ERR_FAIL;
  }

  nng2inf(cmp_pub_sub_device, act_device,
          "name={} start ingress_count={} egress_count={}", name_,
          ingress_endpoints_.size(), egress_endpoints_.size());
  nng_device_aio(aio_.get(), front_.get(), back_.get());
  return z::ERR_OK;
}

void pub_sub_device::stop() {
  bool device_owned_sockets{false};
  if (aio_) {
    aio_.cancel();
    aio_.wait();
    const nng_err result = aio_.result();
    device_owned_sockets = result == NNG_OK || result == NNG_ECANCELED;
    aio_.close();
  }
  if (device_owned_sockets) {
    for (auto &endpoint_listener : ingress_listeners_) {
      (void)endpoint_listener.release();
    }
    for (auto &endpoint_dialer : ingress_dialers_) {
      (void)endpoint_dialer.release();
    }
    for (auto &endpoint_listener : egress_listeners_) {
      (void)endpoint_listener.release();
    }
    for (auto &endpoint_dialer : egress_dialers_) {
      (void)endpoint_dialer.release();
    }
    (void)front_.release();
    (void)back_.release();
  }
  ingress_listeners_.clear();
  ingress_dialers_.clear();
  egress_listeners_.clear();
  egress_dialers_.clear();
  front_.close();
  back_.close();
  ingress_endpoints_.clear();
  egress_endpoints_.clear();
}

const std::vector<endpoint> &pub_sub_device::ingress_endpoints()
    const noexcept {
  return ingress_endpoints_;
}

const std::vector<endpoint> &pub_sub_device::egress_endpoints()
    const noexcept {
  return egress_endpoints_;
}

void pub_sub_device::on_event(void *arg) noexcept {
  auto *self = static_cast<pub_sub_device *>(arg);
  if (self == nullptr) {
    return;
  }
  const auto rv = self->aio_.result();
  if (rv == NNG_OK || rv == NNG_ECANCELED) {
    nng2inf(cmp_pub_sub_device, act_device, "name={} stopped cleanly",
            self->name_);
    return;
  }
  nng2war(cmp_pub_sub_device, act_device,
          "name={} stopped result={} message={}", self->name_,
          static_cast<int>(rv), nng_strerror(rv));
}

} // namespace z::nng
