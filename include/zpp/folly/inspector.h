#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

#include <zpp/namespace.h>

NSB_FOLLY

/**
 * @brief Per-service statistics exported via shared memory.
 *
 * The layout of this struct is shared between the server process and all
 * client processes via a Boost.Interprocess shared memory segment. The
 * @ref version field is used as a lock-free sequence counter to provide a
 * consistent snapshot to readers:
 *   - even value  : stable snapshot is available
 *   - odd value   : writer is in progress
 *
 * Writers increment the version by 1 (to an odd value), update the payload
 * fields, issue a release fence, and finally increment the version again
 * (to the next even value). Readers retry if they observe an odd version or
 * a version change between two loads.
 */
struct alignas(64) ServiceStats {
    /// Monotonic version used as a lock-free sequence counter.
    std::atomic<uint64_t> version{0};
    /// Timestamp of the last update in nanoseconds since an epoch.
    uint64_t last_update_ns{0};
    /// Total threads in the underlying thread pool.
    uint32_t num_threads{0};
    /// Currently active (running) tasks.
    uint32_t num_active{0};
    /// Currently pending (queued) tasks.
    uint32_t num_pending{0};
    /// Reserved for future use / padding.
    uint32_t reserved{0};
};

/**
 * @brief Server-side inspector exporting stats into shared memory.
 *
 * A server process owns an instance of this class and periodically calls
 * @ref update_thread_pool_stats to publish its internal state into a
 * shared memory segment. Consumers can attach via @ref inspector_client
 * from a different process and read snapshots without additional locking.
 */
class inspector_server {
public:
    inspector_server() = default;
    ~inspector_server();
    inspector_server(const inspector_server&) = delete;
    inspector_server& operator=(const inspector_server&) = delete;

    /**
     * @brief Initialize the shared memory segment and construct @ref ServiceStats.
     *
     * This must be called once per process before any update calls. Subsequent
     * calls are no-ops and return @c true as long as initialization succeeded
     * previously.
     *
     * @param shm_name Name of the shared memory object (implementation defined).
     * @return @c true on success, @c false if the shared memory could not be created.
     */
    bool init(std::string_view shm_name);

    /**
     * @brief Release the shared memory mapping and associated resources.
     *
     * Safe to call multiple times; subsequent calls after the first one
     * become no-ops.
     */
    void fini();

    /**
     * @brief Publish the latest thread pool statistics into shared memory.
     *
     * This method implements a lock-free sequence protocol using the
     * @ref ServiceStats::version field to allow readers to obtain a
     * consistent snapshot without taking a lock.
     *
     * @param num_threads Total number of worker threads.
     * @param num_active Number of currently running tasks.
     * @param num_pending Number of queued (pending) tasks.
     */
    void update_thread_pool_stats(uint32_t num_threads,
                                  uint32_t num_active,
                                  uint32_t num_pending);

private:
    /// @brief Name of the shared memory segment backing this server.
    ///
    /// Stored after a successful init() so that fini() can remove the
    /// segment on graceful shutdown. This allows clients to distinguish
    /// between a clean exit (segment removed) and a crash (segment left
    /// behind with the last snapshot).
    std::string _shm_name{};

    ServiceStats* _writable{nullptr};
};

/**
 * @brief Client-side inspector attaching to an existing shared memory segment.
 *
 * A client process uses this class to attach to a @ref inspector_server
 * shared memory segment and read @ref ServiceStats snapshots periodically.
 */
class inspector_client {
public:
    inspector_client() = default;
    ~inspector_client();
    inspector_client(const inspector_client&) = delete;
    inspector_client& operator=(const inspector_client&) = delete;

    /**
     * @brief Attach to an existing shared memory segment in read-only mode.
     *
     * If the client is already initialized, this function returns @c true
     * without performing any additional work.
     *
     * @param shm_name Name of the shared memory object to open.
     * @return @c true on success, @c false if the shared memory cannot be opened.
     */
    bool init(std::string_view shm_name);

    /**
     * @brief Detach from the shared memory segment and release resources.
     *
     * Safe to call multiple times; subsequent calls after the first one
     * become no-ops.
     */
    void fini();

    /**
     * @brief Read a single, self-consistent snapshot of @ref ServiceStats.
     *
     * The implementation uses the versioning protocol described in
     * @ref ServiceStats to avoid torn reads without taking locks.
     *
     * @param out Destination structure to receive the snapshot.
     * @return @c true if a stable snapshot was obtained, @c false otherwise.
     */
    bool read_thread_pool_stats(ServiceStats& out) const;

private:
    const ServiceStats* _readable{nullptr};
};

/**
 * @brief Get the process-wide global inspector_server instance.
 *
 * This is a convenience helper for services that prefer a singleton-style
 * access pattern instead of managing an @ref inspector_server instance
 * explicitly.
 *
 * @return Reference to the global @ref inspector_server instance.
 */
inspector_server& global_inspector_server();

NSE_FOLLY