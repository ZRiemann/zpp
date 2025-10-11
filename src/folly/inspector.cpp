#include <zpp/folly/inspector.h>

#include <thread>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace bip = boost::interprocess;

USE_FOLLY

namespace {

struct shm_handle {
    bip::shared_memory_object shm;
    bip::mapped_region region;
};

inline shm_handle*& server_handle() {
    static shm_handle* h = nullptr;
    return h;
}

inline shm_handle*& client_handle() {
    static shm_handle* h = nullptr;
    return h;
}

constexpr std::size_t stats_size() {
    return sizeof(ServiceStats);
}

} // namespace

inspector_server::~inspector_server() {
    fini();
}

bool inspector_server::init(std::string_view name) {
    if (_writable != nullptr) {
        return true;
    }

    const auto shm_name = std::string{name};
    _shm_name = shm_name;

    auto try_create = [&]() {
        auto* h = new shm_handle{
            bip::shared_memory_object(bip::create_only, shm_name.c_str(), bip::read_write),
            bip::mapped_region{}
        };
        h->shm.truncate(static_cast<bip::offset_t>(stats_size()));
        h->region = bip::mapped_region(h->shm, bip::read_write);

        server_handle() = h;

        void* addr = h->region.get_address();
        _writable = new (addr) ServiceStats{};
    };

    try {
        // First attempt to create the shared memory segment.
        try_create();
        return true;
    } catch (...) {
        // If the shared memory object already exists or creation failed for
        // some other reason, attempt to remove it once and recreate.
        bip::shared_memory_object::remove(shm_name.c_str());
        try {
            try_create();
            return true;
        } catch (...) {
            return false;
        }
    }

    return false;
}

void inspector_server::fini() {
    if (!_writable) {
        return;
    }

    if (!_shm_name.empty()) {
        bip::shared_memory_object::remove(_shm_name.c_str());
    }

    auto*& h = server_handle();
    if (h) {
        delete h;
        h = nullptr;
    }

    _writable = nullptr;
}

inspector_client::~inspector_client() {
    fini();
}

bool inspector_client::init(std::string_view name) {
    if (_readable != nullptr) {
        return true;
    }

    try {
        auto* h = new shm_handle{
            bip::shared_memory_object(bip::open_only, std::string{name}.c_str(), bip::read_only),
            bip::mapped_region{}
        };
        h->region = bip::mapped_region(h->shm, bip::read_only);

        client_handle() = h;

        void* addr = h->region.get_address();
        _readable = static_cast<const ServiceStats*>(addr);
        return true;
    } catch (...) {
        return false;
    }
}

void inspector_client::fini() {
    if (!_readable) {
        return;
    }

    auto*& h = client_handle();
    if (h) {
        delete h;
        h = nullptr;
    }

    _readable = nullptr;
}

void inspector_server::update_thread_pool_stats(uint32_t num_threads,
                                                uint32_t num_active,
                                                uint32_t num_pending) {
    if (!_writable) {
        return;
    }

    auto& s = *_writable;

    auto v1 = s.version.load(std::memory_order_relaxed);
    // mark write in progress (odd version)
    s.version.store(v1 + 1, std::memory_order_relaxed);

    s.num_threads = num_threads;
    s.num_active = num_active;
    s.num_pending = num_pending;
    // last_update_ns can be filled by the caller at a higher layer.

    std::atomic_thread_fence(std::memory_order_release);
    // publish new snapshot (even version)
    s.version.store(v1 + 2, std::memory_order_relaxed);
}

bool inspector_client::read_thread_pool_stats(ServiceStats& out) const {
    if (!_readable) {
        return false;
    }

    const ServiceStats* s = _readable;

    for (int i = 0; i < 3; ++i) {
        auto v1 = s->version.load(std::memory_order_acquire);
        if (v1 % 2 != 0) {
            std::this_thread::yield();
            continue;
        }

        // Take a snapshot of the non-atomic fields while the version is even.
        auto last_update_ns = s->last_update_ns;
        auto num_threads    = s->num_threads;
        auto num_active     = s->num_active;
        auto num_pending    = s->num_pending;

        std::atomic_thread_fence(std::memory_order_acquire);
        auto v2 = s->version.load(std::memory_order_relaxed);
        if (v1 == v2 && v2 % 2 == 0) {
            // 将稳定快照写入输出对象，避免对含 atomic 的结构体做拷贝
            out.version.store(v2, std::memory_order_relaxed);
            out.last_update_ns = last_update_ns;
            out.num_threads    = num_threads;
            out.num_active     = num_active;
            out.num_pending    = num_pending;
            out.reserved       = 0;
            return true;
        }
    }

    return false;
}

inspector_server& z::fo::global_inspector_server() {
    static inspector_server s;
    return s;
}
