# ThreadsafeQueueLib — Benchmark Results

Benchmarks use **Google Benchmark** (v1.8.3), fetched automatically by CMake.
Machine: 12-core @ 4600 MHz, L1 48 KiB, L2 1280 KiB, L3 12288 KiB.

## Build & Run

```bash
# Configure once
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all benchmarks
cmake --build build --target benchmarks -j$(nproc)

# Run
./build/bench_spsc
./build/bench_mpmc
./build/bench_mpsc
```

Useful flags:

```bash
# Filter to specific benchmarks
./build/bench_spsc --benchmark_filter="SingleThread"
./build/bench_spsc --benchmark_filter="Concurrent"
./build/bench_spsc --benchmark_filter="Memory"

# Change time unit
./build/bench_spsc --benchmark_time_unit=ns

# Repeat N times and get mean/stddev
./build/bench_spsc --benchmark_repetitions=5 --benchmark_report_aggregates_only=true

# Suppress CPU-scaling noise (requires sudo)
sudo cpupower frequency-set --governor performance
```

---

## How the metrics are measured

**Latency (single-thread round-trip)**
The benchmark loop body is exactly one `push` followed by one `pop`, run on a single thread with no contention. Google Benchmark runs this millions of times and reports the average time per iteration. This tells you the raw cost of the queue's internal mechanics — atomic operations, memory ordering, and in the unbounded case, heap allocation — with nothing else getting in the way.

**Throughput (concurrent)**
A producer thread and a consumer thread (or multiple of each) run simultaneously. The entire exchange of N items is timed end-to-end using wall-clock time (`UseRealTime`), so any time a thread spends waiting or yielding is counted. `items_per_second` is then derived as `N / wall_time`. This reflects real-world performance under actual thread scheduling pressure, which is why it is always lower than what the single-thread latency would suggest.

**Why the numbers will differ between runs**
Microbenchmarks are sensitive to CPU frequency scaling, OS scheduler decisions, cache state, and background processes. A 10–20% swing between runs on the same machine is completely normal. To get stable numbers, disable CPU frequency scaling (`cpupower frequency-set --governor performance`) and run on an otherwise idle machine.

---

## Results

### SPSC — `lockfree_spsc_bounded` and `lockfree_spsc_unbounded`

| Benchmark | Time | Throughput |
|---|---|---|
| Bounded · single-thread round-trip | **1.57 ns** | 639 M items/s |
| Bounded · single-thread burst (512 items) | 1158 ns/burst | 442 M items/s |
| Bounded · concurrent 1P+1C · 10k items | 179 µs | 56 M items/s |
| Bounded · concurrent 1P+1C · 100k items | 819 µs | 122 M items/s |
| Unbounded · single-thread round-trip | **36.6 ns** | 27 M items/s |
| Unbounded · concurrent 1P+1C · 10k items | 2.8 ms | 3.5 M items/s |
| Unbounded · concurrent 1P+1C · 100k items | 28.6 ms | 3.5 M items/s |

**Key takeaway:** the bounded variant is ~23× faster than the unbounded one in single-thread use because it avoids heap allocation entirely (`new`/`delete` per item vs. a fixed ring buffer). The cost shows up clearly in the concurrent numbers too.

---

### MPMC — `blocking_mpmc_unbounded` and `lockfree_mpmc_bounded`

| Benchmark | Time | Throughput |
|---|---|---|
| Blocking · single-thread round-trip | **102 ns** | 9.8 M items/s |
| Blocking · concurrent 1P+1C | 5.2 ms | 969 k items/s |
| Blocking · concurrent 2P+2C | 18.8 ms | 532 k items/s |
| Blocking · concurrent 4P+4C | 35.1 ms | 569 k items/s |
| Blocking · concurrent 4P+1C | 33.4 ms | 598 k items/s |
| Blocking · concurrent 1P+4C | 6.1 ms | 817 k items/s |
| Lockfree bounded · single-thread round-trip | **28.2 ns** | 35.4 M items/s |
| Lockfree bounded · concurrent 1P+1C | 225 µs | 22.2 M items/s |
| Lockfree bounded · concurrent 2P+2C | 3.0 ms | 3.2 M items/s |
| Lockfree bounded · concurrent 4P+4C | 7.5 ms | 2.7 M items/s |
| Lockfree bounded · concurrent 4P+1C | 5.3 ms | 3.8 M items/s |
| Lockfree bounded · concurrent 1P+4C | 1.9 ms | 2.7 M items/s |

**Key takeaway:** the lockfree bounded queue is ~3.6× faster than the blocking queue in single-thread use (28.2 ns vs 102 ns). However, at high thread counts (4P+4C) the CAS loop under contention degrades, and the gap narrows. The blocking queue is predictable; the lockfree one wins only at low to moderate concurrency.

---

### MPSC — `lockfree_mpsc_unbounded`

| Benchmark | Time | Throughput |
|---|---|---|
| Single-thread round-trip | **45.9 ns** | 21.8 M items/s |
| Concurrent 1P+1C | 1.7 ms | 3.0 M items/s |
| Concurrent 2P+1C | 2.5 ms | 4.0 M items/s |
| Concurrent 4P+1C | 4.6 ms | 4.3 M items/s |
| Concurrent 8P+1C | 9.6 ms | 4.2 M items/s |

**Key takeaway:** throughput scales from 1 to 4 producers (3.0 → 4.3 M/s) but plateaus after that, since the single consumer becomes the bottleneck. The single-thread latency is higher than SPSC unbounded (45.9 ns vs 36.6 ns) because the MPSC push does an atomic exchange on the tail pointer instead of a plain relaxed store.

---

## Queue Comparison Summary

| Queue | Single-thread latency | Best concurrent throughput | Notes |
|---|---|---|---|
| `lockfree_spsc_bounded` | **1.57 ns** | 122 M items/s | Fastest overall; zero allocation |
| `lockfree_mpmc_bounded` | 28.2 ns | 22 M items/s | Good for low-contention MPMC |
| `lockfree_mpsc_unbounded` | 45.9 ns | 4.3 M items/s | Bottlenecked by single consumer |
| `lockfree_spsc_unbounded` | 36.6 ns | 3.5 M items/s | Heap alloc per item hurts under concurrency |
| `blocking_mpmc_unbounded` | 102 ns | 969 k items/s | Highest latency; use only when blocking is acceptable |

> **Note on memory footprint:** The RSS delta benchmarks report 0 KB because the OS allocator reuses pages across iterations within the same process. To get a clean measurement, run the memory benchmark in isolation:
> ```bash
> ./build/bench_spsc --benchmark_filter="Memory" --benchmark_iterations=1
> ```
