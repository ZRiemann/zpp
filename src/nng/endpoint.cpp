#include <zpp/nng/endpoint.h>

#include <filesystem>

namespace z::nng {
namespace {

constexpr std::string_view tcp_prefix = "tcp://";
constexpr std::string_view ipc_prefix = "ipc://";
constexpr std::string_view inproc_prefix = "inproc://";
constexpr std::uint16_t max_tcp_port = 65535;

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

} // namespace

transport parse_transport(std::string_view url) {
    if (is_tcp_url(url)) {
        return transport::tcp;
    }
    if (is_ipc_url(url)) {
        return transport::ipc;
    }
    if (is_inproc_url(url)) {
        return transport::inproc;
    }
    return transport::unknown;
}

bool is_tcp_url(std::string_view url) {
    return starts_with(url, tcp_prefix);
}

bool is_ipc_url(std::string_view url) {
    return starts_with(url, ipc_prefix);
}

bool is_inproc_url(std::string_view url) {
    return starts_with(url, inproc_prefix);
}

std::string make_tcp_url(std::string_view host, std::uint16_t port) {
    return "tcp://" + std::string(host) + ":" + std::to_string(port);
}

bool tcp_port_valid(std::uint32_t port) {
    return port > 0 && port <= max_tcp_port;
}

std::string replace_url_token(std::string_view url, std::string_view from, std::string_view to) {
    std::string result{url};
    const auto pos = result.find(from);
    if (pos == std::string::npos) {
        return result;
    }
    result.replace(pos, from.size(), to);
    return result;
}

std::string ipc_path(std::string_view url) {
    if (!is_ipc_url(url)) {
        return {};
    }
    return std::string(url.substr(ipc_prefix.size()));
}

z::err_t remove_stale_ipc_socket(std::string_view url, std::string* error) {
    if (!is_ipc_url(url)) {
        return z::ERR_OK;
    }

    const auto path = ipc_path(url);
    if (path.empty()) {
        if (error != nullptr) {
            *error = "invalid empty ipc path";
        }
        return z::ERR_FAIL;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return z::ERR_OK;
    }

    std::filesystem::remove(path, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "failed to remove stale ipc socket " + path + ": " + ec.message();
        }
        return z::ERR_FAIL;
    }
    return z::ERR_OK;
}

std::string_view transport_name(transport selected_transport) {
    switch (selected_transport) {
    case transport::tcp:
        return "tcp";
    case transport::ipc:
        return "ipc";
    case transport::inproc:
        return "inproc";
    case transport::unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view endpoint_role_name(endpoint_role role) {
    switch (role) {
    case endpoint_role::listen:
        return "listen";
    case endpoint_role::dial:
        return "dial";
    }
    return "unknown";
}

} // namespace z::nng
