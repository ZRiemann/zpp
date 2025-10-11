#pragma once
#include <zpp/namespace.h>

#include <folly/executors/Executor.h>
#include <folly/executors/GlobalExecutor.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <cuda_runtime.h>

NSB_FOLLY
class executor_cuda : public folly::Executor {
public:
  executor_cuda(int device = 0) {
    _worker = std::thread([this, device] { run(device); });
  }

  ~executor_cuda() {
    {
      std::lock_guard lk(_mu);
      _stopping = true;
      _cv.notify_all();
    }
    if (_worker.joinable()) _worker.join();
  }

  void add(fn_t fn) override {
    {
      std::lock_guard lk(_mu);
      _tasks.push(std::move(fn));
    }
    _cv.notify_one();
  }

  folly::Executor::KeepAlive<> get_keep_alive() {
    return folly::Executor::getKeepAliveToken(this);
  }

private:
  using fn_t = std::function<void()>;

  void run(int device) {
    cudaSetDevice(device);
    cudaStreamCreate(&_stream);
    for (;;) {
      fn_t fn;
      {
        std::unique_lock lk(_mu);
        _cv.wait(lk, [&] { return _stopping || !_tasks.empty(); });
        if (_stopping && _tasks.empty()) break;
        fn = std::move(_tasks.front());
        _tasks.pop();
      }
      // execute on CUDA-thread: submit async ops or run callback
      try {
        fn();
      } catch (...) {
        /* log */
      }
    }
    cudaStreamDestroy(_stream);
  }

  std::thread _worker;
  std::mutex _mu;
  std::condition_variable _cv;
  std::queue<fn_t> _tasks;
  std::atomic<bool> _stopping{false};
  cudaStream_t _stream{nullptr};
};

NSE_FOLLY

#if 0
// 初始化（全局/组件级）：
// Create and keep the executor alive for the process lifetime
// auto cudaExecPtr = std::make_shared<executor_cuda>(/*device=*/0);
// auto cudaKeep = cudaExecPtr->get_keep_alive();

// CPU and IO executors (use Folly globals or your own)
// auto cpuKeep = folly::getGlobalCPUExecutor()->getKeepAliveToken();
// auto ioKeep  = folly::getGlobalIOExecutor()->getKeepAliveToken();

// AsyncScope manages lifetime of all pipeline tasks
// folly::coro::AsyncScope captureScope;

// 管线协程 run_pipeline（CPU -> CUDA -> IO）：
// folly::coro::Task<void> run_pipeline(...)
// 示例略 — 使用 co_reschedule_on(cpuKeep) / co_reschedule_on(cudaKeep) / co_reschedule_on(ioKeep)

// 关于非阻塞高性能：示例实现了一个 cuda 事件 poller（用于在事件完成时 resume 协程），
// 请参考下面的 `cuda_completion_poller` 示例。


#endif
#pragma once
#include <zpp/namespace.h>

#include <folly/executors/Executor.h>
#include <folly/executors/GlobalExecutor.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <cuda_runtime.h>

NSB_FOLLY
class executor_cuda : public folly::Executor {
public:
  executor_cuda(int device = 0) {
    _worker = std::thread([this,device]{ run(device); });
  }
  ~executor_cuda() {
    {
      std::lock_guard lk(_mu);
      _stopping = true;
      _cv.notify_all();
    }
    if(_worker.joinable()) _worker.join();
  }

  void add(fn_t fn) override {
    {
      std::lock_guard lk(_mu);
      _tasks.push(std::move(fn));
    }
    _cv.notify_one();
  }

  folly::Executor::KeepAlive<> get_keep_alive() {
    return folly::Executor::getKeepAliveToken(this);
  }

private:
  using fn_t = std::function<void()>;
  void run(int device) {
    cudaSetDevice(device);
    cudaStreamCreate(&_stream);
    for(;;) {
      fn_t fn;
      {
        std::unique_lock lk(_mu);
        _cv.wait(lk, [&]{ return _stopping || !_tasks.empty(); });
        if (_stopping && _tasks.empty()) break;
        fn = std::move(_tasks.front()); _tasks.pop();
      }
      // execute on CUDA-thread: submit async ops or run callback
      try { fn(); } catch(...) { /* log */ }
    }
    cudaStreamDestroy(_stream);
  }

  std::thread _worker;
  std::mutex _mu;
  std::condition_variable _cv;
  std::queue<fn_t> _tasks;
  std::atomic<bool> _stopping{false};
  cudaStream_t _stream{nullptr};
};

NSE_FOLLY
#if 0
// 初始化（全局/组件级）：
// Create and keep the executor alive for the process lifetime
auto cudaExecPtr = std::make_shared<executor_cuda>(/*device=*/0);
auto cudaKeep = cudaExecPtr->get_keep_alive();

// CPU and IO executors (use Folly globals or your own)
auto cpuKeep = folly::getGlobalCPUExecutor()->getKeepAliveToken();
auto ioKeep  = folly::getGlobalIOExecutor()->getKeepAliveToken();

// AsyncScope manages lifetime of all pipeline tasks
folly::coro::AsyncScope captureScope;

// 管线协程 run_pipeline（CPU -> CUDA -> IO）：
folly::coro::Task<void> run_pipeline(std::unique_ptr<Buffer> buf,
                                     folly::coro::AsyncScope& scope,
                                     folly::Executor::KeepAlive<> cpuKeep,
                                     folly::Executor::KeepAlive<> cudaKeep,
                                     folly::Executor::KeepAlive<> ioKeep,
                                     BufferPool& pool,
                                     Semaphore& inflight) {
  // RAII release & inflight logic
  SCOPE_EXIT { pool.release(std::move(buf)); inflight.release(); };

  // 1) CPU preprocess on CPU executor
  co_await folly::coro::co_reschedule_on(cpuKeep);
  cpu_preprocess(buf->data.data(), buf->data.size());

  // 2) Switch to CUDA thread to submit memcpy + kernel
  co_await folly::coro::co_reschedule_on(cudaKeep);
  // On cuda thread: submit async copy / kernel. Simple mode: synchronize here.
  // Example: cudaMemcpyAsync(..., cudaExecThreadStream) + kernelLaunch + cudaStreamSynchronize
  submit_copy_and_kernel_and_sync(buf->data.data(), buf->data.size());

  // 3) Switch to IO executor to write to disk
  co_await folly::coro::co_reschedule_on(ioKeep);
  folly::writeFile(outputPath.c_str(),
                   reinterpret_cast<const char*>(buf->data.data()),
                   buf->data.size());

  co_return;
}

// 相机 SDK 回调接入（线程安全、非阻塞优先）：
void camera_callback(const uint8_t* src, size_t len) {
  auto buf = pool.try_acquire();
  if (!buf) { ++droppedFrames; return; }            // drop if pool exhausted

  // copy raw frame (small cost compared to processing)
  memcpy(buf->data.data(), src, len);

  if (!inflight.try_acquire()) {                    // limit inflight
    pool.release(std::move(buf));
    ++droppedFrames;
    return;
  }

  // submit coroutine-managed pipeline into AsyncScope
  captureScope.add(run_pipeline(std::move(buf),
                                captureScope,
                                cpuKeep, cudaKeep, ioKeep,
                                pool, inflight));
}

// 优雅关停（取消并等待所有任务完成）：
// Request cancellation of all captured tasks
captureScope.cancel();
// Wait for all children to finish (co_await from a coroutine) or use joinAsync
co_await captureScope.joinAsync(); // if inside a coroutine
// or blocking wait: folly::coro::blockingWait(captureScope.joinAsync());

/*
关于同步 vs 非阻塞完成

简单可靠（推荐先做）：在 CUDA 线程提交 cudaMemcpyAsync + kernel，
    然后 cudaStreamSynchronize(stream_) 在同一 CUDA 线程上等待完成。
    这样不会阻塞 CPU，但 CUDA 线程会被占用直到该操作完成，代码更简单且易于调试。

非阻塞高性能：提交后用 cudaEventRecord 并把 (event, coroutine_handle, resume_executor) 
    注册到一个 CudaCompletionPoller。Poller 在后台检测 cudaEventQuery（或使用 driver callback），
    完成后把 resume post 回预期 executor。实现复杂但能支持更高并发和更低延迟。
*/
#endif

#if 0
// Example: CudaCompletionPoller (simplified, illustrative)
#include <folly/executors/Executor.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/coro/Task.h>
#include <folly/coro/traits.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <cuda_runtime.h>

// Example: cuda_completion_poller (simplified, illustrative)
#include <folly/coro/Task.h>

class cuda_completion_poller {
public:
  struct entry {
    cudaEvent_t event;
    std::coroutine_handle<> handle;
    folly::Executor::KeepAlive<> resume_exec;
  };

  cuda_completion_poller() : _stop(false) { _thr = std::thread([this] { poll_loop(); }); }

  ~cuda_completion_poller() {
    {
      std::lock_guard lk(_mu);
      _stop = true;
      _cv.notify_all();
    }
    if (_thr.joinable()) _thr.join();
    for (auto &e : _entries) cudaEventDestroy(e.event);
  }

  struct awaiter {
    cuda_completion_poller *poller;
    cudaStream_t stream;
    folly::Executor::KeepAlive<> resume_exec;
    cudaEvent_t event{nullptr};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
      cudaEventCreateWithFlags(&event, cudaEventDisableTiming);
      cudaEventRecord(event, stream);

      entry ent;
      ent.event = event;
      ent.handle = h;
      ent.resume_exec = resume_exec;

      {
        std::lock_guard lk(poller->_mu);
        poller->_entries.push_back(ent);
      }
      poller->_cv.notify_one();
    }

    void await_resume() noexcept {}
  };

  awaiter await_event_on_stream(cudaStream_t stream, folly::Executor::KeepAlive<> resume_exec) {
    return awaiter{this, stream, std::move(resume_exec), nullptr};
  }

private:
  void poll_loop() {
    std::unique_lock lk(_mu);
    while (!_stop) {
      _cv.wait_for(lk, std::chrono::milliseconds(1), [this] { return !_entries.empty() || _stop; });
      if (_stop) break;

      auto entries = std::move(_entries);
      _entries.clear();
      lk.unlock();

      std::vector<entry> pending;
      pending.reserve(entries.size());

      for (auto &e : entries) {
        cudaError_t q = cudaEventQuery(e.event);
        if (q == cudaSuccess) {
          auto keep = e.resume_exec;
          auto handle = e.handle;
          keep->add([handle, evt = e.event]() mutable {
            handle.resume();
            cudaEventDestroy(evt);
          });
        } else if (q == cudaErrorNotReady) {
          pending.push_back(e);
        } else {
          auto keep = e.resume_exec;
          auto handle = e.handle;
          keep->add([handle, evt = e.event]() mutable {
            handle.resume();
            cudaEventDestroy(evt);
          });
        }
      }

      lk.lock();
      if (!pending.empty()) {
        _entries.insert(_entries.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
      }
    }
  }

  std::mutex _mu;
  std::condition_variable _cv;
  std::vector<entry> _entries;
  std::thread _thr;
  bool _stop;
};

// Usage in a coroutine pipeline (replacement for co_reschedule_on + sync):
folly::coro::Task<void> run_pipeline_with_poller(
    std::unique_ptr<Buffer> buf,
    cuda_completion_poller& poller,
    folly::Executor::KeepAlive<> cpuKeep,
    folly::Executor::KeepAlive<> cudaKeep,
    folly::Executor::KeepAlive<> ioKeep,
    BufferPool& pool,
    Semaphore& inflight,
    cudaStream_t cudaStream) {

  SCOPE_EXIT { pool.release(std::move(buf)); inflight.release(); };

  // 1) CPU preprocess on CPU executor (same as before)
  //co_await folly::coro::co_reschedule_on(cpuKeep); (discarded for brevity)
  //cpu_preprocess(buf->data.data(), buf->data.size());
  co_await folly::coro::co_withExecutor(
    _keep_cpu, []() -> folly::coro::Task<void> { co_return; }());

  // 2) Submit copy + kernel on the CUDA thread/executor
  // Post to CUDA executor to perform non-blocking cudaMemcpyAsync + kernel launch,
  // then record an event and let poller resume us when event completes.
  co_await folly::coro::co_reschedule_on(cudaKeep);

  // On CUDA thread: submit async ops
  void* d_ptr = allocate_gpu_buffer(buf->data.size()); // user-provided
  cudaMemcpyAsync(d_ptr, buf->data.data(), buf->data.size(), cudaMemcpyHostToDevice, cudaStream);
  launch_kernel(d_ptr, buf->data.size(), cudaStream);

  // Instead of cudaStreamSynchronize (blocking), record event and suspend,
  // letting poller detect completion and resume coroutine on `cudaKeep` or another executor.
  co_await poller.await_event_on_stream(cudaStream, ioKeep /*resume on IO executor*/);

  // 3) Now we are resumed (on IO executor because we requested it), write file
  // If you wanted to resume back to cpuKeep, pass cpuKeep in await_event_on_stream
  co_await folly::coro::co_reschedule_on(ioKeep);
  folly::writeFile(outputPath.c_str(),
                   reinterpret_cast<const char*>(buf->data.data()),
                   buf->data.size());

  co_return;
}
#endif