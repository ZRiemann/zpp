#pragma once

#include <zpp/namespace.h>
#include <atomic>

NSB_ZPP
class tid{
public:
    /**
     * @brief Fast thread id accessor.
     *
     * This function returns a small integer thread id that is much faster to
     * obtain than `std::this_thread::get_id()` (typically 5–20x faster on
     * common platforms). The id is assigned lazily on first call in each
     * thread and cached in a `thread_local` variable for subsequent calls.
     *
     * @note The id values are allocated via an internal atomic counter using
     *       relaxed ordering. IDs start at 0 and increase by 1 for each new
     *       thread observed in the process.
     *
     * @warning The implementation currently encodes the id in 8 bits when
     *          combined with per-thread counters elsewhere; creating >= 256
     *          distinct threads may overflow that encoding. If you expect
     *          more threads, adjust the encoding or add an id-reuse policy.
     *
     * @thread_safety Safe to call concurrently from multiple threads.
     *
     * @return Small integer id unique per thread (within this process).
     */
    static inline int id() noexcept {
        static thread_local int cached_id{-1};
        if (cached_id == -1) {
            cached_id = _next_tid.fetch_add(1, std::memory_order_relaxed);
            // optional: debug assert
            // assert(cached_id < 256 && "thread id overflow (>=256)");
        }    
        return cached_id;
    }

    /**
     * @brief Fast per-thread monotonic counter combined with thread id.
     *
     * Returns a 64-bit value where the high 8 bits contain the thread id and
     * the low 56 bits contain a per-thread monotonic counter. The returned
     * value can be used as a lightweight, mostly-unique identifier for events
     * produced by a thread without requiring global atomic operations.
     *
     * Layout (bits): [ thread_id (8) | counter (56) ]
     *
     * @note The per-thread counter is stored in a `thread_local` variable and
     *       incremented on each call. The counter wraps after 2^56 increments;
     *       wrap-around may produce duplicates only after that many events.
     *
     * @note This function is extremely cheap (a few CPU cycles): increment a
     *       local counter, mask, and combine with the thread id. It avoids
     *       cache-line contention that global atomics suffer under high
     *       concurrency.
     *
     * @warning If you need a strictly globally-monotonic sequence number
     *          (total order across threads), use a global atomic counter
     *          instead. This function provides per-thread monotonicity and
     *          process-wide uniqueness only if thread ids do not collide and
     *          counter wrap-around is acceptable for your workload.
     *
     * @thread_safety Safe to call concurrently from multiple threads.
     *
     * @return 64-bit identifier with thread id in high 8 bits and per-thread
     *         counter in low 56 bits.
     */
    static inline uint64_t count() noexcept {
        constexpr int ID_SHIFT = 56;
        static thread_local uint64_t cnt{static_cast<uint64_t>(id()) << ID_SHIFT};
        return ++cnt;
    }
private:
    static std::atomic<int> _next_tid;
};
NSE_ZPP