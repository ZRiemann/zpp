#pragma once

#include <zpp/nng/aio_ctx.h>
#include <zpp/nng/protocols/protocol.h>
#include <zpp/types.h>

NSB_NNG

/**
 * @brief Multi-endpoint REP0 protocol wrapper with a bounded AIO service pool.
 *
 * Each managed context independently executes a receive/send transaction.
 * Derived classes may override on_receive() and on_send() to implement
 * application behavior; NNG validates the required receive-before-send ordering
 * for every context.
 *
 * @note The caller owns lifecycle synchronization. After configure(), call
 * start() exactly once and call stop() before destruction.
 * @warning start(), configure(), and stop() must not race with each other.
 */
class replier : private endpoint_protocol {
public:
  /// Managed asynchronous context type.
  using aio_ctx = z::nng::aio_ctx;
  /// Asynchronous operation state type.
  using state = z::nng::aio::state;
  /// Constructs an unconfigured replier.
  replier() noexcept;
  /**
   * @brief Destroys the replier.
   * @warning Concurrent mode must already have been stopped with stop().
   */
  virtual ~replier() = default;
  /**
   * @brief Opens a REP0 socket and prepares all configured endpoints.
   * @param endpoints Endpoints transferred into the replier.
   * @param socket_config Optional common socket configuration.
   * @param transport_config Optional transport configuration.
   * @param dialer_config Optional configuration for dial endpoints.
   * @param listener_config Optional configuration for listen endpoints.
   * @return ERR_OK on success; otherwise ERR_FAIL.
   * @pre Concurrent mode is stopped.
   */
  z::err_t configure(std::vector<endpoint> &&endpoints,
                     const socket_options *socket_config = nullptr,
                     const transport_options *transport_config = nullptr,
                     const dialer_options *dialer_config = nullptr,
                     const listener_options *listener_config = nullptr);
  using endpoint_protocol::endpoints;
#pragma region Concurrent AIO Service
  /**
   * @brief Starts the configured endpoints and one receive loop per managed
   * context.
   * @param context_count Number of concurrent request handlers. Zero selects
   * one more than the reported hardware concurrency; explicit values are capped
   * at 128.
   * @return ERR_OK on success; otherwise an endpoint or context initialization
   * error.
   * @pre configure() completed successfully.
   * @note On failure, all endpoints and contexts opened by this call are
   * stopped.
   */
  err_t start(std::size_t context_count = 0);
  /**
   * @brief Stops all managed AIO operations, contexts, and endpoints.
   * @pre Application code no longer accesses the managed service.
   * @note Call before destruction. Do not call from an AIO handler.
   */
  void stop();

protected:
  /**
   * @brief Handles completion of a receive operation for one managed context.
   * @param ctx Context that completed the receive operation.
   * @param result NNG_OK on success; otherwise the asynchronous NNG error.
   * @note On success, the received message remains in ctx.io() until released.
   * An override must consume it and submit the next send or receive operation.
   * @note Calls for different contexts may execute concurrently. Overrides must
   * be thread-safe, must not throw, and should not block or call stop().
   */
  virtual void on_receive(aio_ctx &ctx, nng_err result) noexcept;
  /**
   * @brief Handles completion of a send operation for one managed context.
   * @param ctx Context that completed the send operation.
   * @param result NNG_OK on success; otherwise the asynchronous NNG error.
   * @note After disposing of any message retained by a failed send, an override
   * must submit the next receive operation to keep the service active.
   * @note Calls for different contexts may execute concurrently. Overrides must
   * be thread-safe, must not throw, and should not block or call stop().
   */
  virtual void on_send(aio_ctx &ctx, nng_err result) noexcept;
  /// Contexts owned by concurrent mode; their addresses remain stable while
  /// active.
  std::vector<aio_ctx> aio_ctxs;
  /// Dispatches an AIO completion according to the context's recorded operation
  /// state.
  static void on_event(void *arg, aio_ctx &ctx, state state,
                       nng_err result) noexcept;
#pragma endregion
};

NSE_NNG
