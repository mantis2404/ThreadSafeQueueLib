#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
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

// Single-thread push then pop — measures round-trip latency with no contention
static void BM_SPSC_Bounded_PushPop(benchmark::State& state) {
    lockfree_spsc_bounded<int, 1024> q;
    int val;
    for (auto _ : state) {
        while (!q.try_push(42)) {}
        while (!q.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSC_Bounded_PushPop)->Name("SPSC/Bounded/SingleThread/RoundTripLatency");

// Fill half the queue then drain it — shows burst throughput with cache warmed up
static void BM_SPSC_Bounded_Burst(benchmark::State& state) {
    constexpr size_t N = 512;
    lockfree_spsc_bounded<int, 1024> q;
    int val;
    for (auto _ : state) {
        for (size_t i = 0; i < N; ++i) while (!q.try_push(static_cast<int>(i))) {}
        for (size_t i = 0; i < N; ++i) while (!q.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}
BENCHMARK(BM_SPSC_Bounded_Burst)->Name("SPSC/Bounded/SingleThread/BurstThroughput");

// One producer thread and one consumer thread exchanging N items
static void BM_SPSC_Bounded_Concurrent(benchmark::State& state) {
    const int64_t N = state.range(0);
    for (auto _ : state) {
        lockfree_spsc_bounded<int, 4096> q;

        std::thread prod([&]() {
            for (int i = 0; i < N; ++i)
                while (!q.try_push(i)) std::this_thread::yield();
        });

        int64_t consumed = 0;
        int val;
        while (consumed < N) {
            if (q.try_pop(val)) ++consumed;
            else std::this_thread::yield();
        }
        prod.join();
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_SPSC_Bounded_Concurrent)
    ->Name("SPSC/Bounded/Concurrent/Throughput")
    ->Arg(10000)
    ->Arg(100000)
    ->UseRealTime();

// Single-thread latency for the linked-list (unbounded) variant
static void BM_SPSC_Unbounded_PushPop(benchmark::State& state) {
    lockfree_spsc_unbounded<int> q;
    int val;
    for (auto _ : state) {
        q.push(42);
        while (!q.try_pop(val)) {}
        benchmark::DoNotOptimize(val);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSC_Unbounded_PushPop)->Name("SPSC/Unbounded/SingleThread/RoundTripLatency");

// One producer thread and one consumer thread — unbounded variant
static void BM_SPSC_Unbounded_Concurrent(benchmark::State& state) {
    const int64_t N = state.range(0);
    for (auto _ : state) {
        lockfree_spsc_unbounded<int> q;

        std::thread prod([&]() {
            for (int i = 0; i < N; ++i) q.push(i);
        });

        int64_t consumed = 0;
        int val;
        while (consumed < N) {
            if (q.try_pop(val)) ++consumed;
            else std::this_thread::yield();
        }
        prod.join();
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_SPSC_Unbounded_Concurrent)
    ->Name("SPSC/Unbounded/Concurrent/Throughput")
    ->Arg(10000)
    ->Arg(100000)
    ->UseRealTime();

// Measures how much extra RSS the process gains while holding N queued items
static void BM_SPSC_Unbounded_MemoryFootprint(benchmark::State& state) {
    const int64_t N = state.range(0);
    for (auto _ : state) {
        state.PauseTiming();
        long rss_before = get_rss_kb();
        state.ResumeTiming();

        lockfree_spsc_unbounded<int> q;
        for (int i = 0; i < N; ++i) q.push(i);

        state.PauseTiming();
        long rss_after = get_rss_kb();
        long delta_kb = (rss_after > rss_before) ? (rss_after - rss_before) : 0;
        state.counters["RSS_delta_KB"] = benchmark::Counter(
            static_cast<double>(delta_kb), benchmark::Counter::kAvgIterations);
        state.counters["bytes_per_item"] = benchmark::Counter(
            (N > 0) ? static_cast<double>(delta_kb * 1024) / N : 0.0,
            benchmark::Counter::kAvgIterations);
        int val;
        while (q.try_pop(val)) {}
        state.ResumeTiming();
    }
}
BENCHMARK(BM_SPSC_Unbounded_MemoryFootprint)
    ->Name("SPSC/Unbounded/Memory/Footprint")
    ->Arg(1000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Iterations(3);

BENCHMARK_MAIN();
