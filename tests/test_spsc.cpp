#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>

#include "tsfqueue.hpp"

// TEST 1 & 2: Single-Threaded Sanity Checks

TEST(SPSCQueue, BasicPushPop_Bounded) {
    tsfqueue::__impl::lockfree_spsc_bounded<int, 1024> q;

    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));

    int x;
    EXPECT_TRUE(q.try_pop(x));
    EXPECT_EQ(x, 1);
    EXPECT_TRUE(q.try_pop(x));
    EXPECT_EQ(x, 2);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueue, BasicPushPop_Unbounded) {
    tsfqueue::__impl::lockfree_spsc_unbounded<int> q;;

    EXPECT_TRUE(q.empty());
    q.push(1);
    q.push(2);

    int x;
    EXPECT_TRUE(q.try_pop(x));
    EXPECT_EQ(x, 1);
    EXPECT_TRUE(q.try_pop(x));
    EXPECT_EQ(x, 2);
    EXPECT_TRUE(q.empty());
}

// TEST 3: Bounded Concurrent Stress Test

TEST(SPSCQueue, ConcurrentStress_Bounded) {
    // We use a small capacity (1024) but push 500k items. 
    // This forces the queue to wrap around the array hundreds of times, 
    // heavily testing the index math and collision detection.
    tsfqueue::__impl::lockfree_spsc_bounded<int, 1024> q;
    const int NUM_ITEMS = 500000;

    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            // Keep trying to push until successful (spin-wait)
            while (!q.try_push(i)) {
                std::this_thread::yield(); // Prevent CPU burning if full
            }
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            int val;
            // Keep trying to pop until successful
            while (!q.try_pop(val)) {
                std::this_thread::yield(); // Prevent CPU burning if empty
            }
            // CRITICAL FLAW FIX: We don't just count items. 
            // We verify the exact data wasn't corrupted or re-ordered by thread races.
            EXPECT_EQ(val, i); 
        }
    });

    producer.join();
    consumer.join();
}

// TEST 4: Unbounded Concurrent Stress Test

TEST(SPSCQueue, ConcurrentStress_Unbounded) {
    tsfqueue::__impl::lockfree_spsc_unbounded<int> q;
    const int NUM_ITEMS = 500000;

    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            // Unbounded push never fails, so no while loop needed
            q.push(i);
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            int val;
            while (!q.try_pop(val)) {
                std::this_thread::yield(); 
            }
            EXPECT_EQ(val, i); // shifting dummy node logic is flawless
        }
    });

    producer.join();
    consumer.join();
}