#include <zpp/nng/protocols/respondent.h>
#include <zpp/nng/protocols/surveyor.h>

#include <utility>

#include <zpp/nng/log.h>

namespace z::nng {

surveyor::surveyor() noexcept : endpoint_protocol(cmp_surveyor) {}

z::err_t surveyor::configure(std::vector<endpoint> &&endpoints,
                             const surveyor_options *surveyor_config,
                             const socket_options *socket_config,
                             const transport_options *transport_config,
                             const dialer_options *dialer_config,
                             const listener_options *listener_config) {
  const surveyor_options defaults;
  const auto &selected =
      surveyor_config == nullptr ? defaults : *surveyor_config;
  return endpoint_protocol::configure(
      &socket::surveyor0_open, std::move(endpoints), socket_config,
      transport_config, dialer_config, listener_config,
      &surveyor::configure_socket, &selected);
}

int surveyor::open_context(aio_ctx &context, aio_ctx::callback callback,
                           void *arg,
                           const surveyor_options *options) const noexcept {
  int result = endpoint_protocol::open_context(context, callback, arg);
  if (result != NNG_OK) {
    return result;
  }
  const surveyor_options defaults;
  const auto &selected = options == nullptr ? defaults : *options;
  result = context.ctx().set_ms(NNG_OPT_SURVEYOR_SURVEYTIME,
                                selected.survey_time_ms);
  if (result != NNG_OK) {
    nng2errf(component_, act_configure,
             "failed to configure survey context id={} survey-time={} error={} "
             "message={}",
             context.ctx().id(), selected.survey_time_ms, result,
             nng_strerror(static_cast<nng_err>(result)));
    context.close();
  }
  return result;
}

int surveyor::configure_socket(socket &target, const void *opaque) {
  const auto &options = *static_cast<const surveyor_options *>(opaque);
  return target.set_ms(NNG_OPT_SURVEYOR_SURVEYTIME, options.survey_time_ms);
}

respondent::respondent() noexcept : endpoint_protocol(cmp_respondent) {}

z::err_t respondent::configure(std::vector<endpoint> &&endpoints,
                               const socket_options *socket_config,
                               const transport_options *transport_config,
                               const dialer_options *dialer_config,
                               const listener_options *listener_config) {
  return endpoint_protocol::configure(
      &socket::respondent0_open, std::move(endpoints), socket_config,
      transport_config, dialer_config, listener_config);
}

int respondent::open_context(aio_ctx &context, aio_ctx::callback callback,
                             void *arg) const noexcept {
  return endpoint_protocol::open_context(context, callback, arg);
}

} // namespace z::nng
