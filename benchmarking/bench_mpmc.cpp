#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdio>

#include "tsfqueue.hpp"

using namespace tsfqueue::__impl;

static long get_rss_kb() {
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return -1;
    long rss = -1;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (std::sscanf(line, "VmRSS: %ld kB", &rss) == 1) break;
    }
    std::fclose(f);
    return rss;
}

// Single-thread latency baseline — mutex + condvar overhead with no contention
static void BM_MPMC_Blocking_PushPop(benchmark::State& state) {
    blocking_mpmc_unbounded<int> q;
    int val;
    for (auto _ : state) {
        q.push(42);
        while (!q.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MPMC_Blocking_PushPop)->Name("MPMC/Blocking/SingleThread/RoundTripLatency");

// N producers and M consumers each pushing/popping 5000 items
// state.range(0) = num_producers, state.range(1) = num_consumers
static void BM_MPMC_Blocking_Concurrent(benchmark::State& state) {
    const int np = static_cast<int>(state.range(0));
    const int nc = static_cast<int>(state.range(1));
    const int items_per_prod = 5000;
    const int total = np * items_per_prod;

    for (auto _ : state) {
        blocking_mpmc_unbounded<int> q;
        std::atomic<int> consumed{0};

        std::vector<std::thread> prods, cons;
        for (int i = 0; i < np; ++i)
            prods.emplace_back([&]() {
                for (int j = 0; j < items_per_prod; ++j) q.push(1);
            });
        for (int i = 0; i < nc; ++i)
            cons.emplace_back([&]() {
                while (consumed.fetch_add(1, std::memory_order_relaxed) < total) {
                    int v;
                    q.wait_and_pop(v);
                    benchmark::DoNotOptimize(v);
                }
            });

        for (auto& t : prods) t.join();
        for (auto& t : cons)  t.join();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total));
}
BENCHMARK(BM_MPMC_Blocking_Concurrent)
    ->Name("MPMC/Blocking/Concurrent/Throughput")
    ->Args({1, 1})
    ->Args({2, 2})
    ->Args({4, 4})
    ->Args({4, 1})
    ->Args({1, 4})
    ->UseRealTime();

// RSS delta before vs after holding N items in the blocking queue
static void BM_MPMC_Blocking_Memory(benchmark::State& state) {
    const int64_t N = state.range(0);
    for (auto _ : state) {
        state.PauseTiming();
        long before = get_rss_kb();
        state.ResumeTiming();

        blocking_mpmc_unbounded<int> q;
        for (int i = 0; i < N; ++i) q.push(i);

        state.PauseTiming();
        long after = get_rss_kb();
        long delta = (after > before) ? (after - before) : 0;
        state.counters["RSS_delta_KB"]   = benchmark::Counter(static_cast<double>(delta),
                                               benchmark::Counter::kAvgIterations);
        state.counters["bytes_per_item"] = benchmark::Counter(
            (N > 0) ? static_cast<double>(delta * 1024) / N : 0.0,
            benchmark::Counter::kAvgIterations);
        int v;
        while (q.try_pop(v)) {}
        state.ResumeTiming();
    }
}
BENCHMARK(BM_MPMC_Blocking_Memory)
    ->Name("MPMC/Blocking/Memory/Footprint")
    ->Arg(1000)
    ->Arg(100000)
    ->Arg(500000)
    ->Iterations(3);

// Single-thread latency for the CAS-based bounded variant
static void BM_MPMC_Bounded_PushPop(benchmark::State& state) {
    lockfree_mpmc_bounded<int, 4096> q;
    int val;
    for (auto _ : state) {
        while (!q.try_push(42)) {}
        while (!q.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MPMC_Bounded_PushPop)->Name("MPMC/LockfreeBounded/SingleThread/RoundTripLatency");

// Same NxM producer/consumer grid as the blocking variant
static void BM_MPMC_Bounded_Concurrent(benchmark::State& state) {
    const int np = static_cast<int>(state.range(0));
    const int nc = static_cast<int>(state.range(1));
    const int items_per_prod = 5000;
    const int total = np * items_per_prod;

    for (auto _ : state) {
        lockfree_mpmc_bounded<int, 4096> q;
        std::atomic<int> consumed{0};

        std::vector<std::thread> prods, cons;
        for (int i = 0; i < np; ++i)
            prods.emplace_back([&]() {
                for (int j = 0; j < items_per_prod; ++j)
                    while (!q.try_push(j)) std::this_thread::yield();
            });
        for (int i = 0; i < nc; ++i)
            cons.emplace_back([&]() {
                while (consumed.fetch_add(1, std::memory_order_relaxed) < total) {
                    int v;
                    while (!q.try_pop(v)) std::this_thread::yield();
                    benchmark::DoNotOptimize(v);
                }
            });

        for (auto& t : prods) t.join();
        for (auto& t : cons)  t.join();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total));
}
BENCHMARK(BM_MPMC_Bounded_Concurrent)
    ->Name("MPMC/LockfreeBounded/Concurrent/Throughput")
    ->Args({1, 1})
    ->Args({2, 2})
    ->Args({4, 4})
    ->Args({4, 1})
    ->Args({1, 4})
    ->UseRealTime();

BENCHMARK_MAIN();
