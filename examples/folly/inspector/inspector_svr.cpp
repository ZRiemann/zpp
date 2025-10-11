#include <zpp/folly/inspector.h>

#include <chrono>
#include <thread>
#include <iostream>

USE_FOLLY

int main() {
    auto& insp = global_inspector_server();

    if (!insp.init("zpp_folly_inspector")) {
        std::cerr << "failed to init inspector server shared memory" << std::endl;
        return 1;
    }

    std::cout << "inspector server started" << std::endl;

    // 简单循环模拟状态更新
    for (std::uint32_t i = 0; i < 20; ++i) {
        insp.update_thread_pool_stats(4, i % 4, i * 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    insp.fini();

    std::cout << "inspector server exit" << std::endl;
    return 0;
}
