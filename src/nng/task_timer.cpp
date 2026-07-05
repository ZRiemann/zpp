#include <zpp/core/monitor.h>
#include <zpp/error.h>
#include <zpp/nng/task_timer.h>

USE_ZPP
USE_NNG

task_timer::task_timer(nng_duration d)
    : aio_(&task_timer::aio_handle_cb, this), duration_(d) {}

task_timer::~task_timer() {}

void task_timer::aio_handle_cb(void *h) {
  monitor_guard stat_gurad;
  task_timer *t = static_cast<task_timer *>(h);
  nng_err err = t->aio_.result();
  err_t ret = t->handle(err);
  if (ret == ERR_OK && err == NNG_OK) {
    t->aio_.sleep(t->duration_);
  } // else stop the timer
}

err_t task_timer::handle(nng_err err) {
  // spd_inf("on timer...");
  return ERR_OK;
}

void task_timer::start() { aio_.sleep(duration_); }
void task_timer::stop() { aio_.stop(); }
