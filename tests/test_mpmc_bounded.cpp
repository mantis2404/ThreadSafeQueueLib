#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "lockfree_mpmc_bounded/queue.hpp"

using tsfqueue::__impl::lockfree_mpmc_bounded;

TEST(MPMCBounded, BasicTryPushPop) {
    lockfree_mpmc_bounded<int, 8> q;

    EXPECT_TRUE(q.empty());

    int out = 0;
    EXPECT_FALSE(q.try_pop(out));

    EXPECT_TRUE(q.try_push(10));
    EXPECT_TRUE(q.try_push(20));

    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 10);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 20);
    EXPECT_TRUE(q.empty());
}

TEST(MPMCBounded, FullAndEmptyTransitions) {
    lockfree_mpmc_bounded<int, 4> q;

    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_TRUE(q.try_push(4));

    int out = 0;
    EXPECT_FALSE(q.try_push(5));

    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 1);
    EXPECT_TRUE(q.try_push(5));

    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 2);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 3);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 4);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 5);
    EXPECT_TRUE(q.empty());
}

TEST(MPMCBounded, WrapAroundSingleProducerConsumer) {
    lockfree_mpmc_bounded<int, 4> q;
    const int total = 10000;

    std::thread producer([&]() {
        for (int i = 0; i < total; ++i) {
            while (!q.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < total; ++i) {
            int out = -1;
            while (!q.try_pop(out)) {
                std::this_thread::yield();
            }
            EXPECT_EQ(out, i);
        }
    });

    producer.join();
    consumer.join();
    EXPECT_TRUE(q.empty());
}

TEST(MPMCBounded, WaitAndPopBlocksAndWakes) {
    lockfree_mpmc_bounded<int, 4> q;
    std::atomic<bool> started{false};
    int out = -1;

    std::thread consumer([&]() {
        started.store(true, std::memory_order_release);
        q.wait_and_pop(out);
    });

    while (!started.load(std::memory_order_acquire)) {
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_EQ(out, -1);

    q.wait_and_push(99);
    consumer.join();

    EXPECT_EQ(out, 99);
    EXPECT_TRUE(q.empty());
}

TEST(MPMCBounded, MultiProducerMultiConsumerIntegrity) {
    lockfree_mpmc_bounded<uint64_t, 256> q;

    const int producer_count = 4;
    const int consumer_count = 4;
    const int items_per_producer = 50000;
    const size_t total_items = static_cast<size_t>(producer_count) *
                               static_cast<size_t>(items_per_producer);

    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    for (int p = 0; p < producer_count; ++p) {
        producers.emplace_back([&, p]() {
            const uint64_t base = static_cast<uint64_t>(p) *
                                  static_cast<uint64_t>(items_per_producer);
            for (int i = 0; i < items_per_producer; ++i) {
                uint64_t value = base + static_cast<uint64_t>(i);
                while (!q.try_push(value)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::vector<uint8_t> seen(total_items, 0);
    std::atomic<size_t> consumed{0};
    std::vector<std::thread> consumers;
    consumers.reserve(consumer_count);

    for (int c = 0; c < consumer_count; ++c) {
        consumers.emplace_back([&]() {
            while (consumed.load(std::memory_order_relaxed) < total_items) {
                uint64_t value = 0;
                if (!q.try_pop(value)) {
                    std::this_thread::yield();
                    continue;
                }

                if (value < total_items) {
                    uint8_t &slot = seen[value];
                    EXPECT_EQ(slot, 0);
                    slot = 1;
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto &t : producers) {
        t.join();
    }
    for (auto &t : consumers) {
        t.join();
    }

    for (size_t i = 0; i < total_items; ++i) {
        EXPECT_EQ(seen[i], 1);
    }

    EXPECT_TRUE(q.empty());
}
