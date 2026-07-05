#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <zpp/error.h>
#include <zpp/namespace.h>
#include <zpp/nng/defs.h>

NSB_NNG

/// Transport family selected by an NNG URL.
enum class transport {
  tcp,
  ipc,
  inproc,
  unknown,
};

/// Local socket role for a resolved NNG endpoint.
enum class endpoint_role {
  listen,
  dial,
};

/// Resolved NNG endpoint with role and transport metadata.
struct endpoint {
  /// Full NNG URL.
  std::string url;
  /// Selected transport family.
  z::nng::transport transport{z::nng::transport::unknown};
  /// Whether the local socket listens or dials this endpoint.
  endpoint_role role{endpoint_role::dial};
};

/// Parses the transport family from a full NNG URL.
transport parse_transport(std::string_view url);

/// Returns true when the URL starts with tcp://.
bool is_tcp_url(std::string_view url);

/// Returns true when the URL starts with ipc://.
bool is_ipc_url(std::string_view url);

/// Returns true when the URL starts with inproc://.
bool is_inproc_url(std::string_view url);

/// Formats a TCP NNG URL from a host and port.
std::string make_tcp_url(std::string_view host, std::uint16_t port);

/// Returns true when the port is a valid positive TCP port.
bool tcp_port_valid(std::uint32_t port);

/// Replaces the first matching token in an URL-like string.
std::string replace_url_token(std::string_view url, std::string_view from,
                              std::string_view to);

/// Extracts the filesystem path from an ipc:// URL.
std::string ipc_path(std::string_view url);

/// Removes the filesystem socket path for an ipc:// endpoint if it exists.
z::err_t remove_stale_ipc_socket(std::string_view url, std::string *error);

/// Returns the selected transport as a stable log string.
std::string_view transport_name(transport selected_transport);

/// Returns the endpoint role as a stable log string.
std::string_view endpoint_role_name(endpoint_role role);

/// Validates a single endpoint's URL, transport, and role metadata.
z::err_t validate_endpoint(const endpoint &target);

/**
 * @brief Validates a non-empty list of endpoints.
 * @param endpoints The list of endpoints to validate.
 * @return ERR_OK when every endpoint is valid, otherwise ERR_FAIL.
 */
z::err_t validate_endpoints(const std::vector<endpoint> &endpoints);

NSE_NNG
