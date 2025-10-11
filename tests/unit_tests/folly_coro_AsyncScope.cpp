// Unit tests and examples for folly::coro::AsyncScope and CancellableAsyncScope
#include <gtest/gtest.h>

#include <folly/coro/AsyncScope.h>
#include <folly/coro/Task.h>
#include <folly/coro/BlockingWait.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/coro/CurrentExecutor.h>
#include <zpp/spdlog.h>
#include <zpp/core/monitor.h>

#include <atomic>
#include <thread>
#include <chrono>

using folly::coro::AsyncScope;
using folly::coro::CancellableAsyncScope;
using folly::coro::Task;

// A very small task that increments a counter when executed. It is intended
// to be scheduled on a CPU executor via co_withExecutor before being added
// to an AsyncScope.
static Task<void> shortWork(std::atomic<int>& counter) {
    //spd_inf("shortWork started");
    z::monitor_guard guard;
	++counter;
	co_return;
}

TEST(FollyCoroAsyncScope, JoinWaitsForTasks) {
    // folly::coro::AsyncScope criticalScope(true); //关键 scope（抛出错误）：
    // folly::coro::AsyncScope backgroundScope; // 非关键 scope（记录但不抛）：
	AsyncScope scope;
	std::atomic<int> counter{0};

	// Add several short tasks bound to the global CPU executor.
	for (int i = 0; i < 8; ++i) {
		scope.add(folly::coro::co_withExecutor(
				folly::getGlobalCPUExecutor(), shortWork(counter)));
	}

	// Block until all tasks complete. joinAsync() returns a Task<void> so we
	// use blockingWait in this synchronous test.
	folly::coro::blockingWait(scope.joinAsync());
    z::monitor_guard::print_statistic();
	EXPECT_EQ(counter.load(), 8);
}

// A cancellable task: periodically yields to its executor and checks the
// current cancellation token. When cancellation is requested it exits.
static Task<void> cancellableLoop(std::atomic<int>& iterations) {
	// Retrieve the cancellation token that was attached to this task's promise
	// via co_withCancellation (AsyncScope::add with CancellableAsyncScope).
	const folly::CancellationToken& token = co_await folly::coro::co_current_cancellation_token;

	while (!token.isCancellationRequested()) {
		++iterations;
		// Yield back to the executor to allow cooperative scheduling.
		co_await folly::coro::co_reschedule_on_current_executor;
	}

	co_return;
}

TEST(FollyCoroAsyncScope, CancellableScopeCancels) {
	CancellableAsyncScope scope;
	std::atomic<int> iters{0};

	// Add a long-running cancellable loop bound to the CPU executor. The
	// CancellableAsyncScope::add wrapper will attach the scope's cancellation
	// token to the task via co_withCancellation.
	scope.add(folly::coro::co_withExecutor(
			folly::getGlobalCPUExecutor(), cancellableLoop(iters)));

	// Let the task run for a short while.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::size_t remaining = scope.remaining();
    spd_inf("remaining tasks before cancellation: {}", remaining);

	// Request cancellation and wait for tasks to observe it and finish.
	scope.requestCancellation();
    // scope.cleanup().get(); // or
	folly::coro::blockingWait(scope.cancelAndJoinAsync());

	// The loop should have run at least once and should have stopped after
	// cancellation was requested.
	EXPECT_GT(iters.load(), 0);
}

