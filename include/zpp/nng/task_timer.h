#pragma once

#include "aio.h"
#include "defs.h"
#include <zpp/types.h>
NSB_NNG

/**
 * @class task_timer
 * @note 一个利用nng task threads 定时器
 */
class task_timer {
public:
  task_timer(nng_duration d);
  virtual ~task_timer();

  /**
   * @brief 定时执行任务
   * @retval ERR_OK 继续定时执行
   * @retval !ERR_OK 停止定时执行
   */
  virtual err_t handle(nng_err err);

  void start();
  void stop();

public:
  static void aio_handle_cb(void *h);

public:
  aio aio_;
  nng_duration duration_;
};

NSE_NNG