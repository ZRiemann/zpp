#include <zpp/nng/protocols/pub_sub_device.h>

#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {

pub_sub_device::~pub_sub_device() { stop(); }

z::err_t pub_sub_device::start(std::string name, const endpoint &ingress,
                               const std::vector<endpoint> &egress,
                               const socket_options &options) {
  stop();
  name_ = std::move(name);
  if (ingress.role != endpoint_role::dial || egress.empty()) {
    nng2errf(cmp_pub_sub_device, act_device, "name={} invalid endpoint roles",
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

  if (front_.set_options(options) != NNG_OK ||
      back_.set_options(options) != NNG_OK ||
      front_.pipe_notify(true) != NNG_OK || back_.pipe_notify(true) != NNG_OK) {
    stop();
    return z::ERR_FAIL;
  }
  if (front_.dial(ingress) != z::ERR_OK) {
    stop();
    return z::ERR_FAIL;
  }
  for (const auto &target : egress) {
    if (target.role != endpoint_role::listen ||
        back_.listen(target) != z::ERR_OK) {
      stop();
      return z::ERR_FAIL;
    }
  }

  rv = aio_.create(&pub_sub_device::on_event, this);
  if (rv != NNG_OK) {
    stop();
    return z::ERR_FAIL;
  }

  nng2inf(cmp_pub_sub_device, act_device,
          "name={} start ingress={} egress_count={}", name_, ingress.url,
          egress.size());
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
    (void)front_.release();
    (void)back_.release();
  } else {
    front_.close();
    back_.close();
  }
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
