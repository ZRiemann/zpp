#include "server.h"
#include <zpp/error.h>
#include <zpp/spdlog.h>
#include <zpp/system/sleep.h>
#include <zpp/core/monitor.h>
#include <zpp/folly/io_buf.h>
#include <folly/executors/GlobalExecutor.h>

using folly::coro::Task;
USE_ZPP

app::server* app::server::instance{nullptr};

app::server::server(int argc, char** argv)
    :fo::server(argc, argv){
    instance = this;
}    
app::server::~server(){
    instance = nullptr;
}

err_t app::server::run(){
    fo::server::run();
    spd_inf("app server running...");
    
    return ERR_OK;
}

err_t app::server::stop(){
    spd_inf("app server stopping...");
    return fo::server::stop();
    //return ERR_OK;
}

static std::atomic<int> iters{0};
// A cancellable task: periodically yields to its executor and checks the
// current cancellation token. When cancellation is requested it exits.
static Task<void> cancellableLoop(std::atomic<int>& iterations) {
	// Retrieve the cancellation token that was attached to this task's promise
	// via co_withCancellation (AsyncScope::add with CancellableAsyncScope).
	const folly::CancellationToken& token = co_await folly::coro::co_current_cancellation_token;
    monitor_guard mon_guard;
	while (!token.isCancellationRequested()) {
        spd_inf("cancellableLoop iteration {}", iterations.load());
		++iterations;
        break;
		// Yield back to the executor to allow cooperative scheduling.
		co_await folly::coro::co_reschedule_on_current_executor;
	}

	co_return;
}
// Accept a managed IOBuf which will return memory to the pool on destruction.
Task<void> app::server::mission_cv(std::unique_ptr<folly::IOBuf> buf){
    spd_inf("mission_cv started...");
    co_await cancellableLoop(iters);
    spd_inf("mission_cv completed. size={}", buf ? buf->length() : 0);
    co_return;
}
err_t app::server::on_timer(){
    // 模拟摄像头抓拍图像处理
    static z::fo::io_buf pool(4096 * 2048 * 3, 64);
    auto buf = pool.takeIOBuf();
    // todo: 后续业务逻辑如下
    // 1. 将rgb_row 图像数据进行处理投递给 folly::coro 调度任务
    // 2. folly::coro 并发编排
    //    a. 将rbg_row原始数据保存到本地文件系统
    //    b. 将rgb_row 投递给cuda加速库进行图像分析识别
    // 3  folly::coro 并发编排
    //    将分析结果通过网络发送给远程服务
    
    // mission_cv() 是 prvalue（临时对象）。直接写 add_cpu(mission_cv()) 就是传入一个右值，
    // 模板的转发引用 Awaitable&& 会按需移动，不需要显式 std::move。
    // add_cpu(std::move(mission_cv()));
    add_cpu(mission_cv(std::move(buf)));
    return ERR_OK;
}

err_t app::server::timer(){
    z::sleep_ms(1000);
    return ERR_OK;
}

#define SVR_NAME app::server
#include <zpp/core/main.hpp>
