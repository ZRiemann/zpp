#pragma once

#include <zpp/STL/mpmc.hpp>
#include <zpp/nng/aio_ctx.h>
#include <zpp/nng/protocols/protocol.h>

NSB_NNG

/// Socket-level options for the REQ0 protocol.
struct requester_options {
  /// Delay before an unanswered request is retransmitted.
  nng_duration resend_time_ms{60000};
  /// Frequency used to scan for requests requiring retransmission.
  nng_duration resend_tick_ms{1000};
};

/**
 * @brief Multi-endpoint REQ0 protocol wrapper with bounded asynchronous
 * concurrency.
 *
 * Concurrent mode uses a fixed pool of contexts. Each accepted request
 * exclusively occupies one context until its send and receive sequence
 * completes. Replies may complete out of order and are correlated with requests
 * through the opaque hint supplied to request().
 *
 * @note The caller owns the lifecycle synchronization. After configure(), call
 * start() exactly once before request(). Stop all request-producing threads,
 * then call stop() before destroying the object.
 * @warning start(), request(), configure(), and stop() must not race
 *          with one another. The class intentionally does not add
 * synchronization for invalid lifecycle transitions.
 */
class requester : private endpoint_protocol {
public:
  /// Managed asynchronous context type.
  using aio_ctx = z::nng::aio_ctx;
  /// Asynchronous operation state type.
  using state = z::nng::aio::state;

public:
  /// Constructs an unconfigured requester.
  requester() noexcept;
  /**
   * @brief Destroys the requester.
   * @warning Concurrent mode must already have been stopped with stop().
   * @warning Do not destroy a derived object through a requester pointer
   * because this destructor is intentionally non-virtual.
   */
  ~requester() = default;
  /**
   * @brief Opens a REQ0 socket and prepares all configured endpoints.
   * @param endpoints Endpoints transferred into the requester.
   * @param requester_config Optional REQ0 resend configuration.
   * @param socket_config Optional common socket configuration.
   * @param transport_config Optional transport configuration.
   * @param dialer_config Optional configuration for dial endpoints.
   * @param listener_config Optional configuration for listen endpoints.
   * @return ERR_OK on success; otherwise ERR_FAIL.
   * @pre Concurrent mode is stopped.
   */
  z::err_t configure(std::vector<endpoint> &&endpoints,
                     const requester_options *requester_config = nullptr,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);
  using endpoint_protocol::endpoints;
  /**
   * @brief Starts the configured endpoints and the asynchronous request context
   * pool.
   * @param context_count Maximum number of in-flight requests. Zero selects one
   * more than the reported hardware concurrency; explicit values are capped at
   * 128.
   * @return ERR_OK on success; otherwise an endpoint or context initialization
   * error.
   * @pre configure() completed successfully.
   * @note On failure, all endpoints and contexts opened by this call are
   * stopped.
   */
  err_t start(std::size_t context_count = 0);
  /**
   * @brief Stops all managed AIO operations, contexts, and endpoints.
   * @pre No thread is entering or executing request().
   * @note Call before destruction. Do not call from on_reply().
   */
  void stop();
  /**
   * @brief Submits one asynchronous request using an available managed context.
   * @param msg Request message. Ownership transfers only when the request is
   * accepted.
   * @param hint Opaque correlation value returned unchanged to on_reply(). The
   * caller owns the referenced data and must keep it valid until on_reply()
   * returns.
   * @return NNG_OK when accepted, or NNG_EAGAIN when every context is busy.
   * @pre start() completed successfully and stop() has not started.
   * @note NNG_OK reports local acceptance, not remote delivery or reply
   * success.
   */
  int request(message &msg, void *hint = nullptr) noexcept;

protected:
  /**
   * @brief Handles completion of an accepted asynchronous request.
   * @param msg Received reply on success. On failure, this owns any message
   * retained by the AIO and may be empty; use message::valid() before accessing
   * it.
   * @param result NNG_OK on success; otherwise the asynchronous NNG error.
   * @param hint Opaque correlation value supplied to request().
   * @note The message reference is valid only for the duration of this call.
   * Call message::release() if ownership must outlive the callback.
   * @note Calls for different contexts may execute concurrently on NNG callback
   *       threads. Overrides must be thread-safe, must not throw, and should
   * avoid blocking or calling stop().
   */
  virtual void on_reply(message &msg, nng_err result, void *hint) noexcept;
  /// Applies socket-level REQ0 options during configure().
  static int configure_socket(socket &target, const void *data);
  /// Dispatches an AIO completion to the fixed requester state machine.
  static void on_event(void *arg, aio_ctx &ctx, aio::state state,
                       nng_err result) noexcept;

  /// Dispatches a receive completion to on_reply() and applies the context
  /// reuse policy.
  void on_receive(aio_ctx &ctx, nng_err result) noexcept;
  /// Advances a successful send to receive or reports the send failure.
  void on_send(aio_ctx &ctx, nng_err result) noexcept;

  /// Contexts owned by concurrent mode; their addresses remain stable while
  /// active.
  std::vector<aio_ctx> aio_ctxs;
  /// Pool containing only contexts currently available to request().
  mpmc<aio_ctx *> *ctx_queue_{nullptr};
};

NSE_NNG
