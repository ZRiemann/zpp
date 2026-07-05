#pragma once

#include "defs.h"
#include "log.h"

NSB_NNG

/**
 * @class aio
 * @brief nng asynchronous I/O wrapper
 * @note Tip
 * To get the highest performance with the least overhead,
 * applications should use asynchronous operations described in this chapter
 * whenever possible.
 * @note
 * 异步操作管理：允许应用程序发起I/O操作而不阻塞调用线程
 * 回调处理：当异步操作完成时，执行预先注册的回调函数
 * 结果和错误处理：存储操作的结果和可能的错误代码
 * 超时管理：支持为异步操作设置超时时间 timeout();
 * 取消操作：允许取消正在进行的异步操作 cancellation();
 * 同步流：wait() for it to complete.
 * @note classname T
 * T::perform(); perform the task; 执行当前任务
 * T::advance(); advance the task; 当前任务执行完成后，推进到下一步骤；
 * aio queue to store the tasks;
 *
 */
// template<classname T>
class aio {
public:
  /// @brief The state of the aio.
  enum class state {
    init = 0,
    idle = 1,
    send = 2,
    recv = 3,
    done = 4,
    user1 = 5,
    user2 = 6,
  };
  /// Returns the current state of the aio.
  state get_state() const noexcept { return state_; }
  /// @brief Sets the current state of the aio.
  /// @param s The new state to set.
  void set_state(state s) noexcept { state_ = s; }

public:
  /// Completion callback used by nng_aio_alloc.
  using callback = void (*)(void *);
  /// Cancellation callback used by nng_aio_start.
  using cancel_callback = nng_aio_cancelfn;

  /// Constructs an empty wrapper.
  aio() = default;

  /// Allocates an NNG asynchronous I/O handle.
  aio(callback cb, void *arg) { (void)create(cb, arg); }

  /// Takes ownership of an existing NNG asynchronous I/O handle.
  explicit aio(nng_aio *handle) noexcept : aio_(handle) {}

  /// Frees the owned handle and waits for any active operation to complete.
  ~aio() noexcept { close(); }

  aio(const aio &) = delete;
  aio &operator=(const aio &) = delete;

  /// Moves ownership of an asynchronous I/O handle and its user hint.
  aio(aio &&other) noexcept
      : aio_(other.release()), hint_(other.get_hint()),
        state_(other.get_state()) {
    other.set_hint(nullptr);
    other.set_state(state::done);
  }

  /// Moves ownership of an asynchronous I/O handle and its user hint.
  aio &operator=(aio &&other) noexcept {
    if (this != &other) {
      close();
      aio_ = other.release();
      hint_ = other.get_hint();
      set_state(other.get_state());
      other.set_hint(nullptr);
      other.set_state(state::done);
    }
    return *this;
  }

  /// Allocates a new handle, freeing the previous one if present.
  nng_err create(callback cb = nullptr, void *arg = nullptr) {
    close();
    nng_aio *handle{nullptr};
    const nng_err err = nng_aio_alloc(&handle, cb, arg);
    if (err != NNG_OK) {
      nng2err(cmp_aio, act_create, err);
      return err;
    }
    aio_ = handle;
    set_state(state::init);
    return err;
  }

  /// Returns true when this wrapper owns a valid handle.
  bool valid() const noexcept { return aio_ != nullptr; }

  /// Returns true when this wrapper owns a valid handle.
  explicit operator bool() const noexcept { return valid(); }

  /// Returns the underlying NNG asynchronous I/O handle.
  nng_aio *get() const noexcept { return aio_; }

  /// Releases ownership without freeing the handle.
  nng_aio *release() noexcept {
    nng_aio *released = aio_;
    aio_ = nullptr;
    return released;
  }

  /// Replaces the owned handle, freeing the previous one if present.
  void reset(nng_aio *handle) noexcept {
    if (aio_ == handle) {
      return;
    }
    close();
    aio_ = handle;
    set_state(aio_ == nullptr ? state::done : state::init);
  }

  /// Frees the owned handle and waits for callbacks to finish.
  void close() noexcept {
    nng_aio *handle = release();
    if (handle) {
      nng_aio_free(handle);
    }
    set_state(state::done);
  }

  /// Frees the owned handle asynchronously through NNG's reaper thread.
  void reap() noexcept {
    nng_aio *handle = release();
    if (handle) {
      nng_aio_reap(handle);
    }
    set_state(state::done);
  }

public: // api wrapper
  /// Aborts the current operation with the supplied error.
  inline void abort(nng_err err) noexcept { nng_aio_abort(aio_, err); }
  /// Cancels the current operation with NNG_ECANCELED.
  inline void cancel() noexcept {
    // The nng_aio_cancel function acts like nng_aio_abort, but uses the error
    // code NNG_ECANCELED.
    nng_aio_cancel(aio_);
  }
  /// Permanently stops this handle and waits for callbacks to finish.
  inline void stop() noexcept {
    /*
    The nng_aio_stop function aborts the aio operation with NNG_ESTOPPED,
    and then waits the operation and any associated callback to complete.
    When multiple asynchronous I/O handles are in use and need to be
    deallocated, it is safest to stop all of them using nng_aio_stop, before
    deallocating any of them with nng_aio_free, particularly if the callbacks
    might attempt to reschedule further operations.
    */
    nng_aio_stop(aio_);
  }

  /**
   * @brief The timeout duration is specified as a relative number of
   * milliseconds.
   */
  inline void
  set_timeout(nng_duration timeout = NNG_DURATION_INFINITE) noexcept {
    nng_aio_set_timeout(aio_, timeout);
  }
  /**
   * @brief The nng_aio_set_expire function is similar to nng_aio_set_timeout,
   * but sets an expiration time based on the system clock.
   * The expiration time is a clock timestamp,
   * such as would be returned by nng_clock.
   */
  inline void set_expire(nng_time expiration) noexcept {
    nng_aio_set_expire(aio_, expiration);
  }

  /**
   * @brief sleep
   */
  inline void sleep(nng_duration duration) noexcept {
    set_state(state::idle);
    nng_sleep_aio(duration, aio_);
  }
  /**
   * @brief The nng_aio_wait function waits for an asynchronous I/O operation to
   * complete.
   * @note If the operation has not been started, or has already completed, then
   * it returns immediately.
   */
  inline void wait() noexcept { nng_aio_wait(aio_); }
  /**
   * @brief Waits for completion and returns the operation result.
   */
  inline nng_err wait_result() noexcept {
    wait();
    return result();
  }
  /**
   * @brief Test for Completion
   */
  inline bool busy() const noexcept { return nng_aio_busy(aio_); }

  /**
   * @brief The nng_aio_result function returns the result of the operation
   * associated with the handle aio.
   * @return If the operation was successful, then 0 is returned.
   *  Otherwise a non-zero error code, such as NNG_ECANCELED or NNG_ETIMEDOUT,
   * is returned.
   */
  inline nng_err result() const noexcept { return nng_aio_result(aio_); }
  /**
   * @brief For operations that transfer data
   * @return the number of bytes transferred by the operation associated with
   * the handle aio. Operations that do not transfer data, or do not keep a
   * count, may return zero for this function.
   * @note Either call these from the handle’s completion callback,
   * or after waiting for the operation to complete with nng_aio_wait.
   */
  inline size_t count() const noexcept { return nng_aio_count(aio_); }
#pragma region Messages
  /**
   * @note For send or transmit operations,
   * the rule of thumb is that implementation of the operation is responsible
   * for taking ownership of the message (and releasing resources when it is
   * complete), if it will return success. If the operation will end in error,
   * then the message will be retained and it is the consuming application’s
   * responsibility to dispose of the message. This allows an application
   * the opportunity to reuse the message to try again, if it so desires.
   * 发送成功，nng运行时会回收消息指针；发送失败，消息指针会返回给用户，用户可以继续发送或作销毁等其他处理
   *
   * @note For receive operations, the implementation of the operation will set
   * the message on the aio on success, and the consuming application hasa
   * responsibility to retrieve and dispose of the message. Failure to do so
   * will leak the message. If the operation does not complete successfully,
   * then no message is stored on the aio.
   * 接收成功，用户要读取消息，自行处理。否则消息会泄漏
   */
  /**
   * @brief  retrieve a message in aio.
   */
  inline nng_msg *get_msg() const noexcept { return nng_aio_get_msg(aio_); }
  inline nng_msg *release_msg() noexcept {
    nng_msg *msg = get_msg();
    set_msg(nullptr);
    return msg;
  }
  /**
   * @brief  Store a message in aio.
   */
  inline void set_msg(nng_msg *msg) { nng_aio_set_msg(aio_, msg); }

  /**
   * @brief For some operations, the unit of data transferred is not a message,
   * but rather a stream of bytes.
   * @note A maximum of four (4) nng_iov members may be supplied.
   * @note Most functions using nng_iov do not guarantee to transfer all of the
   * data that they are requested to. To be sure that correct amount of data is
   * transferred, as well as to start an attempt to complete any partial
   * transfer, check the amount of data transferred by calling nng_aio_count.
   */
  inline int set_iov(unsigned int n, const nng_iov *iov) noexcept {
    return nng_aio_set_iov(aio_, n, iov);
  }
  /**
   * @brief Stores a scatter/gather vector in aio.
   */
  inline int set_iov(const nng_iov *iov, unsigned int n) noexcept {
    return set_iov(n, iov);
  }
#pragma endregion
#pragma region Inputs and Outputs
  /**
   * @note The valid values of index range from zero (0) to three (3),
   * as no operation currently defined can accept more than four parameters or
   * return more than four additional results.
   */
  inline void *get_input(unsigned int index) const noexcept {
    return nng_aio_get_input(aio_, index);
  }
  inline void *get_output(unsigned int index) const noexcept {
    return nng_aio_get_output(aio_, index);
  }

  /**
   * @brief Sets an input parameter for an asynchronous operation.
   */
  inline int set_input(unsigned int index, void *param) noexcept {
    return nng_aio_set_input(aio_, index, param);
  }
  /**
   * @brief Sets an output result for an asynchronous operation.
   */
  inline int set_output(unsigned int index, void *result) noexcept {
    return nng_aio_set_output(aio_, index, result);
  }
#pragma endregion
#pragma region I/O Providers
  /**
   * @note These functions will be useful if an application wants to implement
   * its own asynchronous operations using nng_aio.
   */
  /**
   * @brief resets various fields in the aio. It can be called before starting
   * an operation.
   */
  inline void reset() noexcept { nng_aio_reset(aio_); }
  /**
   * @brief starts the operation associated with aio.
   * @note If cb is NULL, then the operation cannot be canceled.
   * @warning Failure to check the return value from nng_aio_start can lead to
   * deadlocks or infinite loops.
   * @retval true then the operation may proceed.
   * @retval false then the aio has been permanently stopped and the operation
   * cannot proceed. In this case the caller should cease all work with aio, and
   * must not call nng_aio_finish or any other function, as the aio is no longer
   * valid for use.
   *
   */
  inline bool start(cancel_callback cb, void *arg) noexcept {
    return nng_aio_start(aio_, cb, arg);
  }
  /**
   * @brief This function causes the callback associated with the aio to called.
   * The aio may not be referenced again by the caller.
   * @note Set any results using nng_aio_set_output before calling this
   * function.
   */
  inline void finish(nng_err err) noexcept { nng_aio_finish(aio_, err); }
#pragma endregion
#pragma region get/set hint
  /**
   * @brief Sets a user-defined hint for the aio.
   * @note This hint can be used to pass user-defined data to the callback
   * function, for example in req/rep mode.
   */
  inline void set_hint(void *hint) noexcept { hint_ = hint; }
  /**
   * @brief Gets the user-defined hint for the aio.
   */
  inline void *get_hint() const noexcept { return hint_; }
public:
  /// Underlying NNG asynchronous I/O handle.
  nng_aio *aio_{nullptr};
  void *hint_{nullptr};
  state state_{state::init};
};
#if 0
// 通用回调包装器
// 快 std::bind %30
// 慢 c call_back %30, 但获得更好的C++分装实现。可以考虑混合使用。
template<typename T>
class cb_wrapper {
public:
    using MemberFuncPtr = void (T::*)();
    
    cb_wrapper(T* instance, MemberFuncPtr func) 
        : m_instance(instance), m_func(func) {}
    
    inline static void callback(void *arg) {
        cb_wrapper<T> *wrapper = static_cast<cb_wrapper<T>*>(arg);
        // 调用成员函数
        (wrapper->m_instance->*(wrapper->m_func))();
    }
    
private:
    T* m_instance;
    MemberFuncPtr m_func;
};

// 使用方式
class MyClass {
public:
    void initAio() {
        // 创建包装器（注意：需要管理其生命周期）
        m_wrapper = new cb_wrapper<MyClass>(this, &MyClass::handleCallback);
        
        // 分配 AIO
        nng_aio *aio;
        nng_aio_alloc(&aio, &cb_wrapper<MyClass>::callback, m_wrapper);
        // 保存 aio 指针...
    }
    
    ~MyClass() {
        delete m_wrapper;
    }

private:
    cb_wrapper<MyClass>* m_wrapper = nullptr;
    
    // 实际处理回调的成员函数
    inline void handleCallback() {
        // 实际的回调处理逻辑
        std::cout << "回调被触发!" << std::endl;
    }
};
#endif
NSE_NNG
