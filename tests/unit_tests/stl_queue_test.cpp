#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include <zpp/STL/mpmc.hpp>
#include <zpp/STL/spsc.hpp>

TEST(SpscQueueTest, InitialChunkNextIsNull) {
    z::spsc<int> queue(4, 2);

    ASSERT_NE(queue.front_chunk_, nullptr);
    ASSERT_NE(queue.back_chunk_, nullptr);
    EXPECT_EQ(queue.front_chunk_->next, nullptr);
    EXPECT_EQ(queue.back_chunk_->next, nullptr);
}

TEST(MpmcQueueTest, InitialChunkNextIsNull) {
    z::mpmc<int> queue(4, 2);

    ASSERT_NE(queue.front_chunk_, nullptr);
    ASSERT_NE(queue.back_chunk_, nullptr);
    EXPECT_EQ(queue.front_chunk_->next, nullptr);
    EXPECT_EQ(queue.back_chunk_->next, nullptr);
}

TEST(SpscQueueTest, PushPopWithinInitialChunkDestructsSafely) {
    {
        z::spsc<int> queue(64, 2);
        for(int value = 0; value < 20; ++value) {
            ASSERT_TRUE(queue.push(value));
        }
        for(int value = 0; value < 20; ++value) {
            int out = -1;
            ASSERT_TRUE(queue.pop(out));
            EXPECT_EQ(out, value);
        }
        EXPECT_TRUE(queue.empty());
    }
}

TEST(MpmcQueueTest, PushPopWithinInitialChunkDestructsSafely) {
    {
        z::mpmc<int> queue(64, 2);
        for(int value = 0; value < 20; ++value) {
            ASSERT_TRUE(queue.push(value));
        }
        for(int value = 0; value < 20; ++value) {
            int out = -1;
            ASSERT_TRUE(queue.pop(out));
            EXPECT_EQ(out, value);
        }
        EXPECT_TRUE(queue.empty());
    }
}

TEST(SpscQueueTest, CrossChunkPushPopPreservesOrder) {
    z::spsc<int> queue(2, 4);

    for(int value = 0; value < 6; ++value) {
        ASSERT_TRUE(queue.push(value));
    }
    for(int value = 0; value < 6; ++value) {
        int out = -1;
        ASSERT_TRUE(queue.pop(out));
        EXPECT_EQ(out, value);
    }
    EXPECT_TRUE(queue.empty());

    auto* spare = queue.spare_chunk_.load(std::memory_order_acquire);
    ASSERT_NE(spare, nullptr);
    EXPECT_EQ(spare->next, nullptr);
}

TEST(MpmcQueueTest, CrossChunkPushPopPreservesOrder) {
    z::mpmc<int> queue(2, 4);

    for(int value = 0; value < 6; ++value) {
        ASSERT_TRUE(queue.push(value));
    }
    for(int value = 0; value < 6; ++value) {
        int out = -1;
        ASSERT_TRUE(queue.pop(out));
        EXPECT_EQ(out, value);
    }
    EXPECT_TRUE(queue.empty());

    auto* spare = queue.spare_chunk_.load(std::memory_order_acquire);
    ASSERT_NE(spare, nullptr);
    EXPECT_EQ(spare->next, nullptr);
}

TEST(MpmcQueueTest, SingleProducerSingleConsumerStress) {
    z::mpmc<int> queue(8, 4);
    std::atomic<bool> producer_done{false};
    constexpr int kCount = 1000;

    std::thread producer([&]() {
        for(int value = 0; value < kCount; ++value) {
            while(!queue.push(value)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        for(int expected = 0; expected < kCount;) {
            int out = -1;
            if(queue.pop(out)) {
                EXPECT_EQ(out, expected);
                ++expected;
            } else {
                EXPECT_FALSE(producer_done.load(std::memory_order_acquire));
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
    EXPECT_TRUE(queue.empty());
}
