#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <numeric>

#include "tsfqueue.hpp" 

using namespace tsfqueue::__impl;

// BASIC FUNCTIONALITY TEST
// Checks: push, try_pop, empty, size

TEST(MPMCUnbounded, BasicOperations) {
    blocking_mpmc_unbounded<int> q;

    EXPECT_TRUE(q.empty());
    q.push(1);
    q.push(2);
    EXPECT_EQ(q.size(), 2u);
    EXPECT_FALSE(q.empty());

    int val;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(q.empty());
}


//  MOVE-ONLY TYPES & EMPLACE
//  Checks: Does the queue handle types that CANNOT be copied (like unique_ptr)?
//  This tests if your internal logic correctly uses std::move and std::forward.

TEST(MPMCUnbounded, MoveOnlyTypes) {
    blocking_mpmc_unbounded<std::unique_ptr<int>> q;

    q.push(std::make_unique<int>(42));
    q.emplace_back(std::make_unique<int>(100));

    std::unique_ptr<int> out;
    EXPECT_TRUE(q.try_pop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, 42);

    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(*out, 100);
}


//  BLOCKING BEHAVIOR
//  Checks: Does wait_and_pop actually wait for the producer?

TEST(MPMCUnbounded, BlockingWait) {
    blocking_mpmc_unbounded<int> q;
    std::atomic<bool> ready{false};
    int result = 0;

    std::thread consumer([&]() {
        ready.store(true);
        q.wait_and_pop(result); // Should sleep here
    });

    // Wait for consumer thread to start
    while(!ready.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(result, 0); // Still 0 because nothing pushed
    q.push(99);
    
    consumer.join();
    EXPECT_EQ(result, 99);
}

//  MPMC STRESS TEST (DATA INTEGRITY)
//  Checks: Data race conditions, lost wakeups, and total sum integrity.
 
TEST(MPMCUnbounded, HighContentionStress) {
    blocking_mpmc_unbounded<int> q;
    
    const int num_producers = 4;
    const int num_consumers = 4;
    const int items_per_prod = 10000;
    const int total_items = num_producers * items_per_prod;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::atomic<long long> total_sum{0};

    // Producers: Push integers 1..total_items
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < items_per_prod; ++j) {
                q.push(1); // Simplest case: push '1' total_items times
            }
        });
    }

    // Consumers: Pop and add to total_sum
    std::atomic<int> consumed_count{0};
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&]() {
            while (consumed_count.fetch_add(1) < total_items) {
                int val;
                q.wait_and_pop(val);
                total_sum.fetch_add(val);
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    // Sum should exactly equal total_items since we pushed '1' every time
    EXPECT_EQ(total_sum.load(), (long long)total_items);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}