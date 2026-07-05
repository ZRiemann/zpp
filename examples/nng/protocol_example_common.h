#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <zpp/error.h>
#include <zpp/json.hpp>
#include <zpp/namespace.h>
#include <zpp/nng/aio.h>
#include <zpp/nng/dialer.h>
#include <zpp/nng/endpoint.h>
#include <zpp/nng/listener.h>
#include <zpp/nng/message.h>
#include <zpp/nng/socket.h>
#include <zpp/nng/task_timer.h>
#include <zpp/spdlog.h>

NSB_APP

/// Parsed common configuration for an endpoint-based protocol example.
struct protocol_example_config {
  /// Endpoints assigned the role selected by the example.
  std::vector<nng::endpoint> endpoints;
  /// Optional common socket configuration.
  std::optional<nng::socket_options> socket;
  /// Optional TCP transport configuration.
  std::optional<nng::transport_options> transport;
  /// Optional dialer configuration.
  std::optional<nng::dialer_options> dialer;
  /// Optional listener configuration.
  std::optional<nng::listener_options> listener;
};

/// Parsed configuration for the publisher-device-subscriber topology.
struct pub_sub_example_config {
  /// Device ingress endpoint, configured with the dial role.
  nng::endpoint ingress;
  /// Device egress endpoints, configured with the listen role.
  std::vector<nng::endpoint> egress;
  /// Subscriber topic filters.
  std::vector<std::string> topics;
  /// Optional common socket configuration.
  std::optional<nng::socket_options> socket;
  /// Optional TCP transport configuration.
  std::optional<nng::transport_options> transport;
  /// Optional dialer configuration.
  std::optional<nng::dialer_options> dialer;
  /// Optional listener configuration.
  std::optional<nng::listener_options> listener;
};

/// Loads common socket, transport, dialer, and listener options.
template <typename Config>
inline err_t load_protocol_example_options(json_view &section, Config &parsed) {
  json_view options;
  auto result = section.member("socket_options", options);
  if (result == ERR_OK) {
    nng::socket_options socket;
    if (options.get("recv_queue_depth", socket.recv_buffer) != ERR_OK ||
        options.get("send_queue_depth", socket.send_buffer) != ERR_OK ||
        options.get("recv_timeout_ms", socket.recv_timeout_ms) != ERR_OK ||
        options.get("send_timeout_ms", socket.send_timeout_ms) != ERR_OK) {
      return ERR_FAIL;
    }
    parsed.socket = socket;
  } else if (result != ERR_NOT_EXIST) {
    return ERR_FAIL;
  }

  result = section.member("transport_options", options);
  if (result == ERR_OK) {
    json_view tcp;
    nng::transport_options transport;
    if (options.member("tcp", tcp) != ERR_OK ||
        tcp.get("nodelay", transport.tcp_no_delay) != ERR_OK ||
        tcp.get("keepalive", transport.tcp_keepalive) != ERR_OK) {
      return ERR_FAIL;
    }
    parsed.transport = transport;
  } else if (result != ERR_NOT_EXIST) {
    return ERR_FAIL;
  }

  result = section.member("dialer_options", options);
  if (result == ERR_OK) {
    nng::dialer_options dialer;
    int max_recv_size_bytes{0};
    if (options.get("reconnect_min_ms", dialer.reconnect_min_ms) != ERR_OK ||
        options.get("reconnect_max_ms", dialer.reconnect_max_ms) != ERR_OK ||
        options.get("max_recv_size_bytes", max_recv_size_bytes) != ERR_OK ||
        max_recv_size_bytes < 0) {
      return ERR_FAIL;
    }
    dialer.max_recv_size_bytes = static_cast<std::size_t>(max_recv_size_bytes);
    parsed.dialer = dialer;
  } else if (result != ERR_NOT_EXIST) {
    return ERR_FAIL;
  }

  result = section.member("listener_options", options);
  if (result == ERR_OK) {
    int max_recv_size_bytes{0};
    if (options.get("max_recv_size_bytes", max_recv_size_bytes) != ERR_OK ||
        max_recv_size_bytes < 0) {
      return ERR_FAIL;
    }
    parsed.listener =
        nng::listener_options{static_cast<std::size_t>(max_recv_size_bytes)};
  } else if (result != ERR_NOT_EXIST) {
    return ERR_FAIL;
  }
  return ERR_OK;
}

/// Loads one protocol section from the nng_net object in a JSON file.
inline err_t load_protocol_example_config(const char *path,
                                          const char *section_name,
                                          nng::endpoint_role role,
                                          protocol_example_config &output) {
  json document;
  json_view nng_net;
  json_view section;
  if (document.load_file(path) != ERR_OK ||
      document.member("nng_net", nng_net) != ERR_OK ||
      nng_net.member(section_name, section) != ERR_OK) {
    return ERR_FAIL;
  }

  json_view endpoint_values;
  if (section.member("endpoints", endpoint_values) != ERR_OK ||
      !endpoint_values.is_array() || endpoint_values.size() == 0) {
    return ERR_FAIL;
  }
  protocol_example_config parsed;
  parsed.endpoints.reserve(endpoint_values.size());
  for (std::size_t index = 0; index < endpoint_values.size(); ++index) {
    json_view value;
    std::string url;
    if (endpoint_values.at(index, value) != ERR_OK ||
        value.get(url) != ERR_OK) {
      return ERR_FAIL;
    }
    const auto selected_transport = nng::parse_transport(url);
    if (selected_transport == nng::transport::unknown) {
      return ERR_FAIL;
    }
    parsed.endpoints.push_back({std::move(url), selected_transport, role});
  }

  if (load_protocol_example_options(section, parsed) != ERR_OK) {
    return ERR_FAIL;
  }

  rapidjson::StringBuffer buffer;
  spd_inf("nng_net.{}: {}", section_name, section.to_string(buffer, true));
  output = std::move(parsed);
  return ERR_OK;
}

/// Loads the shared publisher-device-subscriber topology configuration.
inline err_t load_pub_sub_example_config(const char *path,
                                         pub_sub_example_config &output) {
  json document;
  json_view nng_net;
  json_view section;
  if (document.load_file(path) != ERR_OK ||
      document.member("nng_net", nng_net) != ERR_OK ||
      nng_net.member("pub_sub", section) != ERR_OK) {
    return ERR_FAIL;
  }

  pub_sub_example_config parsed;
  std::string ingress_url;
  if (section.get("ingress_endpoint", ingress_url) != ERR_OK) {
    return ERR_FAIL;
  }
  const auto ingress_transport = nng::parse_transport(ingress_url);
  if (ingress_transport == nng::transport::unknown) {
    return ERR_FAIL;
  }
  parsed.ingress = {std::move(ingress_url), ingress_transport,
                    nng::endpoint_role::dial};

  json_view egress_values;
  if (section.member("egress_endpoints", egress_values) != ERR_OK ||
      !egress_values.is_array() || egress_values.size() == 0) {
    return ERR_FAIL;
  }
  parsed.egress.reserve(egress_values.size());
  for (std::size_t index = 0; index < egress_values.size(); ++index) {
    json_view value;
    std::string url;
    if (egress_values.at(index, value) != ERR_OK || value.get(url) != ERR_OK) {
      return ERR_FAIL;
    }
    const auto selected_transport = nng::parse_transport(url);
    if (selected_transport == nng::transport::unknown) {
      return ERR_FAIL;
    }
    parsed.egress.push_back(
        {std::move(url), selected_transport, nng::endpoint_role::listen});
  }

  json_view topic_values;
  if (section.member("topics", topic_values) != ERR_OK ||
      !topic_values.is_array() || topic_values.size() == 0) {
    return ERR_FAIL;
  }
  parsed.topics.reserve(topic_values.size());
  for (std::size_t index = 0; index < topic_values.size(); ++index) {
    json_view value;
    std::string topic;
    if (topic_values.at(index, value) != ERR_OK || value.get(topic) != ERR_OK) {
      return ERR_FAIL;
    }
    parsed.topics.push_back(std::move(topic));
  }

  if (load_protocol_example_options(section, parsed) != ERR_OK) {
    return ERR_FAIL;
  }
  rapidjson::StringBuffer buffer;
  spd_inf("nng_net.pub_sub: {}", section.to_string(buffer, true));
  output = std::move(parsed);
  return ERR_OK;
}

/// Periodically sends an incrementing counter through a send-only protocol.
template <typename Protocol>
class protocol_counter_sender : public nng::task_timer {
public:
  /// Creates a periodic sender bound to a protocol instance.
  protocol_counter_sender(nng_duration interval, Protocol &protocol)
      : nng::task_timer(interval), protocol_(protocol) {}

private:
  err_t handle(nng_err /*error*/) override {
    nng::message message{std::size_t{0}};
    if (message.append_u32(counter_) != NNG_OK) {
      return ERR_FAIL;
    }
    const int result = protocol_.send(message);
    if (result != NNG_OK && result != NNG_EAGAIN) {
      spd_err("protocol send failed: [{}] {}", result,
              nng_strerror(static_cast<nng_err>(result)));
      return ERR_FAIL;
    }
    if (result == NNG_OK) {
      spd_inf("sent counter: {}", counter_++);
    }
    return ERR_OK;
  }

  std::uint32_t counter_{0};
  Protocol &protocol_;
};

/// Owns one reusable AIO and continuously receives counter messages.
template <typename Protocol> class protocol_counter_receiver {
public:
  /// Binds the receiver to a receive-capable protocol.
  explicit protocol_counter_receiver(Protocol &protocol) noexcept
      : protocol_(protocol) {}

  /// Allocates the AIO and starts receiving.
  int start() {
    const int result = io_.create(&protocol_counter_receiver::complete, this);
    if (result == NNG_OK) {
      protocol_.recv(io_);
    }
    return result;
  }

  /// Stops active receive work and waits for its callback.
  void stop() noexcept {
    if (io_) {
      io_.stop();
    }
  }

private:
  static void complete(void *arg) {
    static_cast<protocol_counter_receiver *>(arg)->on_complete();
  }

  void on_complete() {
    const nng_err result = io_.result();
    if (result == NNG_OK) {
      nng_msg *message = io_.get_msg();
      std::uint32_t counter{0};
      if (message != nullptr && nng_msg_chop_u32(message, &counter) == NNG_OK) {
        spd_inf("received counter: {}", counter);
      } else {
        spd_err("failed to decode counter message");
      }
      nng_msg_free(message);
      io_.set_msg(nullptr);
    } else if (result == NNG_ESTOPPED || result == NNG_ECANCELED ||
               result == NNG_ECLOSED) {
      return;
    } else if (result != NNG_ETIMEDOUT) {
      spd_err("protocol receive failed: [{}] {}", static_cast<int>(result),
              nng_strerror(result));
    }
    protocol_.recv(io_);
  }

  Protocol &protocol_;
  nng::aio io_;
};

NSE_APP
