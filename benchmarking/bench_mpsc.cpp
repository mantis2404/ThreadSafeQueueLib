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

// Single-thread latency — no contention, measures pure push + pop overhead
static void BM_MPSC_Unbounded_PushPop(benchmark::State& state) {
    lockfree_mpsc_unbounded<int> q;
    int val;
    for (auto _ : state) {
        q.push(42);
        while (!q.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MPSC_Unbounded_PushPop)->Name("MPSC/Unbounded/SingleThread/RoundTripLatency");

// N producers pushing to a single consumer — state.range(0) = num_producers
static void BM_MPSC_Unbounded_Concurrent(benchmark::State& state) {
    const int np = static_cast<int>(state.range(0));
    const int items_per_prod = 5000;
    const int total = np * items_per_prod;

    for (auto _ : state) {
        lockfree_mpsc_unbounded<int> q;
        std::atomic<int64_t> consumed{0};

        std::vector<std::thread> prods;
        for (int i = 0; i < np; ++i)
            prods.emplace_back([&]() {
                for (int j = 0; j < items_per_prod; ++j) q.push(j);
            });

        std::thread consumer([&]() {
            int v;
            while (consumed.load(std::memory_order_relaxed) < total) {
                if (q.try_pop(v)) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    benchmark::DoNotOptimize(v);
                } else {
                    std::this_thread::yield();
                }
            }
        });

        for (auto& t : prods) t.join();
        consumer.join();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(total));
}
BENCHMARK(BM_MPSC_Unbounded_Concurrent)
    ->Name("MPSC/Unbounded/Concurrent/Throughput")
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

// RSS delta before vs after holding N items in the queue
static void BM_MPSC_Unbounded_Memory(benchmark::State& state) {
    const int64_t N = state.range(0);
    for (auto _ : state) {
        state.PauseTiming();
        long before = get_rss_kb();
        state.ResumeTiming();

        lockfree_mpsc_unbounded<int> q;
        for (int i = 0; i < N; ++i) q.push(i);

        state.PauseTiming();
        long after = get_rss_kb();
        long delta = (after > before) ? (after - before) : 0;
        state.counters["RSS_delta_KB"]   = benchmark::Counter(
            static_cast<double>(delta), benchmark::Counter::kAvgIterations);
        state.counters["bytes_per_item"] = benchmark::Counter(
            (N > 0) ? static_cast<double>(delta * 1024) / N : 0.0,
            benchmark::Counter::kAvgIterations);
        int v;
        while (q.try_pop(v)) {}
        state.ResumeTiming();
    }
}
BENCHMARK(BM_MPSC_Unbounded_Memory)
    ->Name("MPSC/Unbounded/Memory/Footprint")
    ->Arg(1000)
    ->Arg(100000)
    ->Arg(500000)
    ->Iterations(3);

BENCHMARK_MAIN();
