#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "lockfree_mpsc_unbounded/queue.hpp"

using tsfqueue::__impl::lockfree_mpsc_unbounded;

TEST(MPSCQueue, BasicSingleProducerConsumer) {
    lockfree_mpsc_unbounded<int> q;

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    q.push(10);
    q.push(20);
    q.push(30);

    int out = 0;
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 10);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 20);
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 30);

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(MPSCQueue, PeekDoesNotConsume) {
    lockfree_mpsc_unbounded<int> q;

    q.push(42);

    int peeked = 0;
    ASSERT_TRUE(q.peek(peeked));
    EXPECT_EQ(peeked, 42);
    EXPECT_EQ(q.size(), 1u);

    int popped = 0;
    ASSERT_TRUE(q.try_pop(popped));
    EXPECT_EQ(popped, 42);
    EXPECT_TRUE(q.empty());
}

TEST(MPSCQueue, WaitAndPopBlocksAndWakes) {
    lockfree_mpsc_unbounded<int> q;
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

    q.push(99);
    consumer.join();

    EXPECT_EQ(out, 99);
    EXPECT_TRUE(q.empty());
}

TEST(MPSCQueue, MultiProducerUniqueItemIntegrity) {
    lockfree_mpsc_unbounded<uint64_t> q;

    const int producer_count = 6;
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
                q.push(base + static_cast<uint64_t>(i));
            }
        });
    }

    std::vector<uint8_t> seen(total_items, 0);
    size_t consumed = 0;

    while (consumed < total_items) {
        uint64_t value = 0;
        if (!q.try_pop(value)) {
            std::this_thread::yield();
            continue;
        }

        ASSERT_LT(value, total_items);
        ASSERT_EQ(seen[value], 0) << "duplicate value " << value;
        seen[value] = 1;
        ++consumed;
    }

    for (auto &t : producers) {
        t.join();
    }

    for (size_t i = 0; i < total_items; ++i) {
        ASSERT_EQ(seen[i], 1) << "missing value " << i;
    }

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

static double run_mpsc_bottleneck_case(int producer_count,
                                       int items_per_producer,
                                       uint64_t *consumed_sum) {
    lockfree_mpsc_unbounded<uint64_t> q;
    const size_t total_items = static_cast<size_t>(producer_count) *
                               static_cast<size_t>(items_per_producer);

    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    auto t0 = std::chrono::steady_clock::now();

    for (int p = 0; p < producer_count; ++p) {
        producers.emplace_back([&, p]() {
            const uint64_t base = static_cast<uint64_t>(p) *
                                  static_cast<uint64_t>(items_per_producer);
            for (int i = 0; i < items_per_producer; ++i) {
                q.push(base + static_cast<uint64_t>(i));
            }
        });
    }

    uint64_t local_sum = 0;
    size_t consumed = 0;
    while (consumed < total_items) {
        uint64_t v = 0;
        if (!q.try_pop(v)) {
            std::this_thread::yield();
            continue;
        }
        local_sum += v;
        ++consumed;
    }

    for (auto &t : producers) {
        t.join();
    }

    auto t1 = std::chrono::steady_clock::now();
    const double seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0)
            .count();

    if (consumed_sum != nullptr) {
        *consumed_sum = local_sum;
    }

    return seconds;
}

TEST(MPSCQueue, BottleneckProfileByProducerCount) {
    const int items_per_producer = 100000;
    const std::vector<int> producer_counts = {1, 2, 4, 8};

    for (int producers : producer_counts) {
        const size_t total = static_cast<size_t>(producers) *
                             static_cast<size_t>(items_per_producer);
        uint64_t observed_sum = 0;
        const double seconds =
            run_mpsc_bottleneck_case(producers, items_per_producer, &observed_sum);

        const uint64_t n = static_cast<uint64_t>(total);
        const uint64_t expected_sum = (n * (n - 1)) / 2;

        ASSERT_EQ(observed_sum, expected_sum);
        ASSERT_GT(seconds, 0.0);

        const double throughput_mops =
            (static_cast<double>(total) / seconds) / 1e6;

        std::cout << "[MPSC bottleneck] producers=" << producers
                  << " total_items=" << total
                  << " time_s=" << seconds
                  << " throughput_Mops=" << throughput_mops << std::endl;
    }
}
