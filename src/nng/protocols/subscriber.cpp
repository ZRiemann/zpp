#include <zpp/nng/protocols/subscriber.h>

#include <utility>

#include <zpp/core/monitor.h>
#include <zpp/nng/log.h>

namespace z::nng {

struct subscriber::configure_data {
  const std::vector<std::string> &topics;
  const subscriber_options &options;
};

subscriber::subscriber() noexcept : endpoint_protocol(cmp_subscriber) {}

z::err_t subscriber::configure(std::vector<endpoint> &&endpoints,
                               const std::vector<std::string> &topics,
                               const subscriber_options *subscriber_config,
                               const socket_options *socket_config,
                               const transport_options *transport_config,
                               const dialer_options *dialer_config,
                               const listener_options *listener_config) {
  if (topics.empty()) {
    nng2errf(cmp_subscriber, act_validate, "topic list is empty");
    stop();
    return z::ERR_FAIL;
  }
  const subscriber_options defaults;
  const configure_data data{
      topics, subscriber_config == nullptr ? defaults : *subscriber_config};
  return endpoint_protocol::configure(
      &socket::sub0_open, std::move(endpoints), socket_config, transport_config,
      dialer_config, listener_config, &subscriber::configure_socket, &data);
}

z::err_t subscriber::start(std::size_t aio_count) {
  const z::err_t start_result = endpoint_protocol::start();
  if (start_result != z::ERR_OK) {
    return start_result;
  }

  const std::size_t normalized_count = normalize_concurrency(aio_count);
  receive_aios_.resize(normalized_count);
  for (std::size_t index = 0; index < receive_aios_.size(); ++index) {
    auto &io = receive_aios_[index];
    const int result = io.create(&subscriber::on_event, &io);
    if (result != NNG_OK) {
      nng2errf(component_, act_start,
               "failed to create managed AIO index={} aios={} error={} "
               "message={}",
               index, normalized_count, result,
               nng_strerror(static_cast<nng_err>(result)));
      stop();
      return result;
    }
    io.set_hint(this);
  }
  for (auto &io : receive_aios_) {
    recv(io);
  }
  return z::ERR_OK;
}

void subscriber::stop() {
  for (auto &io : receive_aios_) {
    if (io) {
      io.stop();
    }
  }
  receive_aios_.clear();
  endpoint_protocol::stop();
}

void subscriber::on_receive(message &, nng_err result) noexcept {
  if (result != NNG_OK && result != NNG_ETIMEDOUT) {
    nng2err(component_, act_recv, result);
  }
}

void subscriber::on_event(void *arg) noexcept {
  auto *io = static_cast<aio *>(arg);
  if (io == nullptr) {
    nng2errf(cmp_subscriber, act_callback, "missing AIO callback argument");
    return;
  }
  auto *owner = static_cast<subscriber *>(io->get_hint());
  if (owner == nullptr) {
    nng2errf(cmp_subscriber, act_callback, "missing subscriber owner hint");
    message release{io->release_msg()};
    return;
  }
  const nng_err result = io->result();
  message msg{io->release_msg()};
  if (result == NNG_ESTOPPED || result == NNG_ECLOSED ||
      result == NNG_ECANCELED) {
    return;
  }

  monitor_guard guard;
  owner->on_receive(msg, result);
  owner->recv(*io);
}

int subscriber::configure_socket(socket &target, const void *opaque) {
  const auto &data = *static_cast<const configure_data *>(opaque);
  int result = target.set_bool(NNG_OPT_SUB_PREFNEW, data.options.prefer_new);
  for (std::size_t index = 0; index < data.topics.size(); ++index) {
    if (result != NNG_OK) {
      break;
    }
    const auto &topic = data.topics[index];
    result =
        nng_sub0_socket_subscribe(target.get(), topic.data(), topic.size());
    if (result != NNG_OK) {
      nng2errf(cmp_subscriber, act_configure,
               "failed to subscribe topic index={} size={} error={} message={}",
               index, topic.size(), result,
               nng_strerror(static_cast<nng_err>(result)));
    }
  }
  return result;
}

} // namespace z::nng
