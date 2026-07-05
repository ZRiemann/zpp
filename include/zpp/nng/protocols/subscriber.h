#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <zpp/nng/protocols/protocol.h>

NSB_NNG

/// SUB0 protocol-specific configuration.
struct subscriber_options {
  /// Keep the newest message when the receive queue is full.
  bool prefer_new{true};
};

/**
 * @brief Multi-endpoint SUB0 wrapper with managed asynchronous receives.
 *
 * All receive slots use the socket's default context, so each matching message
 * completes exactly one pending AIO. Derived classes process completions
 * through on_receive(); the slot is resubmitted automatically after the
 * callback returns.
 *
 * @note The caller owns lifecycle synchronization. After configure(), call
 * start() exactly once and call stop() before destruction.
 * @warning start(), configure(), and stop() must not race with each other.
 */
class subscriber : private endpoint_protocol {
public:
  /// Constructs an unconfigured subscriber.
  subscriber() noexcept;
  /**
   * @brief Destroys the subscriber.
   * @warning Managed receive mode must already have been stopped with stop().
   */
  virtual ~subscriber() = default;

  /**
   * @brief Opens a SUB0 socket and configures its default subscription context.
   * @param endpoints Endpoints transferred into the subscriber.
   * @param topics Non-empty topic prefixes; an empty prefix subscribes to all
   * messages.
   * @param subscriber_config Optional SUB0 queue preference configuration.
   * @param socket_config Optional common socket configuration.
   * @param transport_config Optional transport configuration.
   * @param dialer_config Optional configuration for dial endpoints.
   * @param listener_config Optional configuration for listen endpoints.
   * @return ERR_OK on success; otherwise ERR_FAIL.
   * @pre Managed receive mode is stopped.
   */
  z::err_t configure(std::vector<endpoint> &&endpoints,
                     const std::vector<std::string> &topics,
                     const subscriber_options *subscriber_config = nullptr,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);

  using endpoint_protocol::endpoints;
  /**
   * @brief Starts endpoints and a fixed pool of pending receive AIOs.
   * @param aio_count Number of concurrent receive handlers. Zero selects one
   * more than the reported hardware concurrency; explicit values are capped at
   * 128.
   * @return ERR_OK on success; otherwise an endpoint or AIO initialization
   * error.
   * @pre configure() completed successfully.
   * @note On failure, all AIOs and endpoints opened by this call are stopped.
   */
  z::err_t start(std::size_t aio_count = 0);
  /**
   * @brief Stops all managed receives and endpoints.
   * @pre Application code no longer accesses the managed subscriber.
   * @note Call before destruction. Do not call from on_receive().
   */
  void stop();

protected:
  /**
   * @brief Handles one receive completion.
   * @param msg Received message on success; otherwise any message retained by
   * the AIO.
   * @param result NNG_OK on success; otherwise the asynchronous NNG error.
   * @note The message is valid only for this call unless release() transfers
   * ownership.
   * @note Calls may execute concurrently. Overrides must be thread-safe, must
   * not throw, and should avoid blocking or calling stop().
   */
  virtual void on_receive(message &msg, nng_err result) noexcept;

private:
  struct configure_data;
  static int configure_socket(socket &target, const void *data);
  static void on_event(void *arg) noexcept;

  /// Managed AIOs whose addresses remain stable while receives are active.
  std::vector<aio> receive_aios_;
};

NSE_NNG
