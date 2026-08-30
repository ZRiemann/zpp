#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <utility>

namespace z {

/// Coordinates admission and draining of concurrent activities.
///
/// lifecycle is a generic synchronization primitive. It does not own threads,
/// tasks, queues, sockets, runtimes, or business resources. Callers acquire a
/// guard before starting work that must complete before the protected resource
/// can be stopped. Shutdown first quiesces admission, then waits for all guards
/// to be released, stops the protected resource, and finally marks the
/// lifecycle stopped.
///
/// @warning The lifecycle object must outlive every guard acquired from it.
/// @warning Destruction requires the lifecycle to be stopped with no active
/// guards.
class lifecycle {
public:
  /// Logical admission state of the protected object or service.
  enum class state : std::uint8_t {
    /// No activity may be acquired and the lifecycle may be started.
    stopped = 0,
    /// New activities may be acquired.
    running = 1,
    /// New activities are rejected while existing activities drain.
    quiescing = 2,
  };

  /// Move-only RAII lease for one admitted activity.
  class guard {
  public:
    /// Constructs an empty guard that does not hold an activity.
    guard() noexcept = default;

    /// Releases the held activity, if any.
    ~guard() noexcept { release(); }

    guard(const guard &) = delete;
    guard &operator=(const guard &) = delete;

    /// Transfers one activity lease from another guard.
    guard(guard &&other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)) {}

    /// Releases the current activity and transfers another lease.
    guard &operator=(guard &&other) noexcept {
      if (this != &other) {
        release();
        owner_ = std::exchange(other.owner_, nullptr);
      }
      return *this;
    }

    /// Returns true when this guard owns one admitted activity.
    explicit operator bool() const noexcept { return owner_ != nullptr; }

    /// Releases the admitted activity before destruction.
    void release() noexcept {
      lifecycle *owner = std::exchange(owner_, nullptr);
      if (owner != nullptr) {
        owner->release_activity();
      }
    }

  private:
    friend class lifecycle;

    explicit guard(lifecycle *owner) noexcept : owner_(owner) {}

    lifecycle *owner_{nullptr};
  };

  /// Constructs a stopped lifecycle with no active activities.
  lifecycle() noexcept = default;

  /// Verifies that no activity outlives the lifecycle.
  ~lifecycle() noexcept {
    assert(active_count() == 0);
    assert(current_state() == state::stopped);
  }

  lifecycle(const lifecycle &) = delete;
  lifecycle &operator=(const lifecycle &) = delete;
  lifecycle(lifecycle &&) = delete;
  lifecycle &operator=(lifecycle &&) = delete;

  /// Starts admission for a stopped lifecycle.
  /// @return true when the lifecycle changed to running; false otherwise.
  bool start() noexcept {
    control_type expected = encode(state::stopped, 0);
    return control_.compare_exchange_strong(
        expected, encode(state::running, 0), std::memory_order_acq_rel,
        std::memory_order_acquire);
  }

  /// Attempts to admit one activity.
  /// @return A valid guard while running; an empty guard otherwise.
  [[nodiscard]] guard try_acquire() noexcept {
    control_type current = control_.load(std::memory_order_acquire);
    for (;;) {
      if (decode_state(current) != state::running) {
        return {};
      }

      const control_type active = decode_active(current);
      if (active == active_mask) {
        return {};
      }

      const control_type desired = encode(state::running, active + 1);
      if (control_.compare_exchange_weak(
              current, desired, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        return guard{this};
      }
    }
  }

  /// Stops admitting new activities while allowing admitted work to drain.
  ///
  /// The operation is idempotent. Calling it while stopped is a no-op.
  void quiesce() noexcept {
    control_type current = control_.load(std::memory_order_acquire);
    for (;;) {
      const state selected = decode_state(current);
      if (selected == state::stopped || selected == state::quiescing) {
        return;
      }

      const control_type desired =
          encode(state::quiescing, decode_active(current));
      if (control_.compare_exchange_weak(
              current, desired, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        if (decode_active(desired) == 0) {
          notify_drained();
        }
        return;
      }
    }
  }

  /// Waits until admission is closed and all admitted activities are released.
  /// @pre quiesce() has been called, or the lifecycle is already stopped.
  void wait_drained() {
    std::unique_lock lock{wait_mutex_};
    wait_cv_.wait(lock, [this] { return drained(); });
  }

  /// Waits for the lifecycle to drain for at most the requested duration.
  /// @param timeout Maximum duration to wait.
  /// @return true when drained before the timeout; false otherwise.
  template <class Rep, class Period>
  bool wait_drained_for(
      const std::chrono::duration<Rep, Period> &timeout) {
    std::unique_lock lock{wait_mutex_};
    return wait_cv_.wait_for(lock, timeout, [this] { return drained(); });
  }

  /// Marks a drained lifecycle stopped.
  ///
  /// This operation never waits. Call quiesce() and wait_drained() before
  /// stopping any protected resource, then call stop() after that resource is
  /// no longer active.
  /// @return true when stopped or already stopped; false if still running or
  /// activities remain.
  bool stop() noexcept {
    control_type current = control_.load(std::memory_order_acquire);
    for (;;) {
      const state selected = decode_state(current);
      if (selected == state::stopped) {
        return true;
      }
      if (selected != state::quiescing || decode_active(current) != 0) {
        return false;
      }

      const control_type desired = encode(state::stopped, 0);
      if (control_.compare_exchange_weak(
              current, desired, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        return true;
      }
    }
  }

  /// Returns the current admission state.
  state current_state() const noexcept {
    return decode_state(control_.load(std::memory_order_acquire));
  }

  /// Returns the number of currently admitted activities.
  std::uint64_t active_count() const noexcept {
    return decode_active(control_.load(std::memory_order_acquire));
  }

  /// Returns true when new activities may be acquired.
  bool accepting() const noexcept {
    return current_state() == state::running;
  }

  /// Returns true when admission is closed and no activity remains.
  bool drained() const noexcept {
    const control_type current = control_.load(std::memory_order_acquire);
    return decode_state(current) != state::running &&
           decode_active(current) == 0;
  }

private:
  using control_type = std::uint64_t;

  static constexpr unsigned state_shift = 62;
  static constexpr control_type active_mask =
      (control_type{1} << state_shift) - 1;
  static constexpr control_type state_mask = ~active_mask;

  static constexpr control_type encode(state selected,
                                       control_type active) noexcept {
    return (static_cast<control_type>(selected) << state_shift) |
           (active & active_mask);
  }

  static constexpr state decode_state(control_type control) noexcept {
    return static_cast<state>((control & state_mask) >> state_shift);
  }

  static constexpr control_type decode_active(control_type control) noexcept {
    return control & active_mask;
  }

  void release_activity() noexcept {
    const control_type previous =
        control_.fetch_sub(1, std::memory_order_acq_rel);
    const control_type previous_active = decode_active(previous);
    assert(previous_active != 0);
    assert(decode_state(previous) != state::stopped);

    if (previous_active == 1 &&
        decode_state(previous) == state::quiescing) {
      notify_drained();
    }
  }

  void notify_drained() noexcept {
    std::lock_guard lock{wait_mutex_};
    wait_cv_.notify_all();
  }

  std::atomic<control_type> control_{encode(state::stopped, 0)};
  mutable std::mutex wait_mutex_;
  mutable std::condition_variable wait_cv_;
};

} // namespace z
