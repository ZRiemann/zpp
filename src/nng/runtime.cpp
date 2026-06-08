#include <zpp/nng/runtime.h>

#include <chrono>
#include <utility>

#include <zpp/spdlog.h>

namespace z::nng {
namespace {

void log_nng_error(std::string_view context, int rv) {
    spd_err("[nng:{}] error={} message={}", context, rv, nng_strerror(static_cast<nng_err>(rv)));
}

int open_socket(socket& target, std::string_view context, int (*open_fn)(nng_socket*)) {
    nng_socket opened{};
    const auto rv = open_fn(&opened);
    if (rv != 0) {
        log_nng_error(context, rv);
        return rv;
    }
    target.reset(opened);
    spd_inf("[nng:{}] id={}", context, target.id());
    return rv;
}

} // namespace

std::int64_t now_ns() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

socket::socket(nng_socket opened)
    : sock_(opened)
    , valid_(true) {
}

socket::~socket() {
    close();
}

socket::socket(socket&& other) noexcept
    : sock_(other.sock_)
    , valid_(other.valid_) {
    other.valid_ = false;
}

socket& socket::operator=(socket&& other) noexcept {
    if (this != &other) {
        close();
        sock_ = other.sock_;
        valid_ = other.valid_;
        other.valid_ = false;
    }
    return *this;
}

bool socket::valid() const {
    return valid_;
}

nng_socket socket::get() const {
    return sock_;
}

void socket::close() {
    if (!valid_) {
        return;
    }
    const auto rv = nng_socket_close(sock_);
    if (rv != 0) {
        log_nng_error("close", rv);
    }
    valid_ = false;
    sock_ = NNG_SOCKET_INITIALIZER;
}

void socket::reset(nng_socket opened) {
    close();
    sock_ = opened;
    valid_ = true;
}

int socket::bus0_open() {
    return open_socket(*this, "bus0-open", nng_bus0_open);
}

int socket::pub0_open() {
    return open_socket(*this, "pub0-open", nng_pub0_open);
}

int socket::pub0_open_raw() {
    return open_socket(*this, "pub0-raw-open", nng_pub0_open_raw);
}

int socket::sub0_open() {
    return open_socket(*this, "sub0-open", nng_sub0_open);
}

int socket::sub0_open_raw() {
    return open_socket(*this, "sub0-raw-open", nng_sub0_open_raw);
}

int socket::pull0_open() {
    return open_socket(*this, "pull0-open", nng_pull0_open);
}

int socket::push0_open() {
    return open_socket(*this, "push0-open", nng_push0_open);
}

int socket::rep0_open() {
    return open_socket(*this, "rep0-open", nng_rep0_open);
}

int socket::req0_open() {
    return open_socket(*this, "req0-open", nng_req0_open);
}

int socket::respondent0_open() {
    return open_socket(*this, "respondent0-open", nng_respondent0_open);
}

int socket::surveyor0_open() {
    return open_socket(*this, "surveyor0-open", nng_surveyor0_open);
}

int socket::pair0_open() {
    return open_socket(*this, "pair0-open", nng_pair0_open);
}

int socket::pair1_open() {
    return open_socket(*this, "pair1-open", nng_pair1_open);
}

int socket::pair1_open_poly() {
    return open_socket(*this, "pair1-poly-open", nng_pair1_open_poly);
}

void socket::apply_options(const socket_options& options) {
    (void)nng_socket_set_ms(sock_, NNG_OPT_SENDTIMEO, options.send_timeout_ms);
    (void)nng_socket_set_ms(sock_, NNG_OPT_RECVTIMEO, options.recv_timeout_ms);
    (void)nng_socket_set_int(sock_, NNG_OPT_SENDBUF, options.send_buffer);
    (void)nng_socket_set_int(sock_, NNG_OPT_RECVBUF, options.recv_buffer);
    (void)nng_socket_set_bool(sock_, NNG_OPT_TCP_NODELAY, options.tcp_no_delay);
}

z::err_t socket::listen(const endpoint& target) {
    if (!valid_) {
        spd_err("[nng:listen] invalid socket");
        return z::ERR_FAIL;
    }
    std::string error;
    if (remove_stale_ipc_socket(target.url, &error) != z::ERR_OK) {
        spd_err("[nng:listen] {}", error);
        return z::ERR_FAIL;
    }
    const auto rv = nng_listen(sock_, target.url.c_str(), nullptr, 0);
    if (rv != 0) {
        log_nng_error("listen", rv);
        spd_err("[nng:listen] url={}", target.url);
        return z::ERR_FAIL;
    }
    spd_inf("[nng:listen] transport={} url={}", transport_name(target.transport), target.url);
    return z::ERR_OK;
}

z::err_t socket::dial(const endpoint& target, bool nonblock) {
    if (!valid_) {
        spd_err("[nng:dial] invalid socket");
        return z::ERR_FAIL;
    }
    const int flags = nonblock ? NNG_FLAG_NONBLOCK : 0;
    const auto rv = nng_dial(sock_, target.url.c_str(), nullptr, flags);
    if (rv != 0) {
        log_nng_error("dial", rv);
        spd_err("[nng:dial] url={}", target.url);
        return z::ERR_FAIL;
    }
    spd_inf("[nng:dial] transport={} url={}", transport_name(target.transport), target.url);
    return z::ERR_OK;
}

z::err_t publisher::start(const z::nng::endpoint& target, const socket_options& options) {
    stop();
    if (target.role != endpoint_role::listen || target.transport == transport::inproc) {
        spd_err("[nng:publisher] invalid publisher endpoint role={} transport={} url={}",
                endpoint_role_name(target.role),
                transport_name(target.transport),
                target.url);
        return z::ERR_FAIL;
    }

    const auto rv = socket_.pub0_open();
    if (rv != 0) {
        return z::ERR_FAIL;
    }
    socket_.apply_options(options);
    if (socket_.listen(target) != z::ERR_OK) {
        stop();
        return z::ERR_FAIL;
    }
    endpoint_ = target.url;
    return z::ERR_OK;
}

int publisher::send(nng_msg* message) {
    if (message == nullptr) {
        return NNG_EINVAL;
    }
    if (!socket_.valid()) {
        nng_msg_free(message);
        return NNG_ECLOSED;
    }
    const auto rv = nng_sendmsg(socket_.get(), message, NNG_FLAG_NONBLOCK);
    if (rv != 0) {
        nng_msg_free(message);
    }
    return rv;
}

void publisher::stop() {
    socket_.close();
    endpoint_.clear();
}

bool publisher::running() const {
    return socket_.valid();
}

const std::string& publisher::endpoint() const {
    return endpoint_;
}

const std::string& publisher::endpoint_url() const {
    return endpoint_;
}

event_forwarder::~event_forwarder() {
    stop();
}

z::err_t event_forwarder::start(std::string name,
                                const endpoint& ingress,
                                const std::vector<endpoint>& egress,
                                const socket_options& options) {
    stop();
    name_ = std::move(name);
    if (ingress.role != endpoint_role::dial || egress.empty()) {
        spd_err("[nng:{}:device] invalid endpoint roles", name_);
        return z::ERR_FAIL;
    }

    auto rv = front_.sub0_open_raw();
    if (rv != 0) {
        return z::ERR_FAIL;
    }

    rv = back_.pub0_open_raw();
    if (rv != 0) {
        stop();
        return z::ERR_FAIL;
    }

    front_.apply_options(options);
    back_.apply_options(options);
    if (front_.dial(ingress) != z::ERR_OK) {
        stop();
        return z::ERR_FAIL;
    }
    for (const auto& target : egress) {
        if (target.role != endpoint_role::listen || back_.listen(target) != z::ERR_OK) {
            stop();
            return z::ERR_FAIL;
        }
    }

    rv = nng_aio_alloc(&aio_, &event_forwarder::on_device_done, this);
    if (rv != 0) {
        log_nng_error("aio-alloc", rv);
        stop();
        return z::ERR_FAIL;
    }

    running_.store(true);
    spd_inf("[nng:{}:device] start ingress={} egress_count={}", name_, ingress.url, egress.size());
    nng_device_aio(aio_, front_.get(), back_.get());
    return z::ERR_OK;
}

void event_forwarder::stop() {
    running_.store(false);
    if (aio_ != nullptr) {
        nng_aio_stop(aio_);
        nng_aio_free(aio_);
        aio_ = nullptr;
    }
    front_.close();
    back_.close();
}

bool event_forwarder::running() const {
    return running_.load();
}

void event_forwarder::on_device_done(void* arg) {
    auto* self = static_cast<event_forwarder*>(arg);
    if (self == nullptr) {
        return;
    }
    const auto rv = nng_aio_result(self->aio_);
    self->running_.store(false);
    if (rv == 0) {
        spd_inf("[nng:{}:device] stopped cleanly", self->name_);
        return;
    }
    spd_war("[nng:{}:device] stopped result={} message={}",
            self->name_,
            static_cast<int>(rv),
            nng_strerror(rv));
}

} // namespace z::nng
