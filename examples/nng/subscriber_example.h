#pragma once

#include <zpp/nng/engine.h>
#include <zpp/nng/protocols/subscriber.h>

NSB_APP

/// Concurrent subscriber that decodes published counter messages.
class counter_subscriber : public nng::subscriber {
protected:
  /// Decodes one receive completion; the base class continues receiving.
  void on_receive(nng::message &msg, nng_err result) noexcept override;
};

/// Example application receiving counters from a PUB/SUB device.
class subscriber_example : public nng::engine {
public:
  /// Creates the subscriber example application.
  subscriber_example(int argc, char **argv);

  /// Loads and applies the shared PUB/SUB topology configuration.
  err_t configure() override;
  /// Starts subscriber endpoints and asynchronous receives.
  err_t run() override;
  /// Stops receive activity and subscriber resources.
  err_t stop() override;

private:
  counter_subscriber subscriber_;
};

NSE_APP
