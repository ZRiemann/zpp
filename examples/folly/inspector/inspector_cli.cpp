#include <zpp/folly/inspector.h>

#include <chrono>
#include <thread>
#include <iostream>

USE_FOLLY

int main() {
    inspector_client insp;

    if (!insp.init("zpp_folly_inspector")) {
        std::cerr << "failed to init inspector client shared memory" << std::endl;
        return 1;
    }

    std::cout << "inspector client started" << std::endl;

    for (int i = 0; i < 20; ++i) {
        ServiceStats stats{};
        if (insp.read_thread_pool_stats(stats)) {
            std::cout << "threads=" << stats.num_threads
                      << ", active=" << stats.num_active
                      << ", pending=" << stats.num_pending << std::endl;
        } else {
            std::cout << "failed to read stats snapshot" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "inspector client exit" << std::endl;
    return 0;
}
