# ThreadSafeQueueLib

ThreadSafeQueueLib is a header-only C++ queue library for safe communication between threads.

It gives you multiple queue designs so you can pick the right trade-off between latency, blocking behavior, and producer/consumer topology.

## Why this project is useful

When multiple threads share data, writing your own queue can easily introduce races, deadlocks, or poor performance. This library helps by providing ready-to-use queue implementations for common patterns:

- **SPSC**: single producer, single consumer
- **MPSC**: multiple producers, single consumer
- **MPMC**: multiple producers, multiple consumers
- **Bounded / Unbounded**: fixed-capacity ring buffers or dynamically growing queues
- **Lock-free / Blocking variants** depending on use case

## Key features

- Header-only API in `include/`
- C++17 compatible (as configured in CMake)
- Queue operations such as `try_push`, `try_pop`, `wait_and_push`, `wait_and_pop`, `push`, `peek`, and `size` (availability depends on queue type)
- Stress-tested with GoogleTest-based concurrent tests

## Project structure

```text
ThreadSafeQueueLib/
├── CMakeLists.txt                     # Build + test configuration
├── README.md                          # Project documentation
├── include/
│   ├── tsfqueue.hpp                   # Aggregated include for main queue headers
│   ├── utils.hpp                      # Shared node/cache-line utilities
│   ├── lockfree_spsc_bounded/         # Lock-free bounded SPSC queue
│   ├── lockfree_spsc_unbounded/       # Lock-free unbounded SPSC queue
│   ├── lockfree_mpsc_unbounded/       # Lock-free unbounded MPSC queue
│   ├── lockfree_mpmc_bounded/         # Lock-free bounded MPMC queue
│   ├── blocking_mpmc_unbounded/       # Blocking unbounded MPMC queue
│   └── FAST_lockfree_spsc_unbounded/  # Experimental fast SPSC implementation
├── tests/
│   ├── test_spsc.cpp
│   ├── test_mpsc.cpp
│   ├── test_mpmc.cpp
│   └── test_mpmc_bounded.cpp
├── examples/
│   └── examples.md                    # Placeholder for usage examples
└── benchmarking/
    └── benchmark.md                   # Placeholder for benchmarks
```

> Empty folders are intentionally ignored in this layout.

## Available queue types

- `tsfqueue::__impl::lockfree_spsc_bounded<T, Capacity>`
- `tsfqueue::__impl::lockfree_spsc_unbounded<T>`
- `tsfqueue::__impl::lockfree_mpsc_unbounded<T>`
- `tsfqueue::__impl::lockfree_mpmc_bounded<T, Capacity>`
- `tsfqueue::__impl::blocking_mpmc_unbounded<T>`

## Quick start

### 1) Build tests

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### 2) Include in your code

```cpp
#include "tsfqueue.hpp"
```

### 3) Choose a queue by your threading model

- If exactly one producer and one consumer: use an **SPSC** queue.
- If many producers feed one consumer: use **MPSC**.
- If many producers and many consumers share work: use **MPMC**.
- If memory usage must be strictly controlled: use a **bounded** queue.
- If you prefer backpressure via blocking calls: use a queue with `wait_and_*` operations.

## How it helps beginners

- Provides production-style queue patterns without needing to implement lock-free internals yourself.
- Gives concrete test files that demonstrate expected behavior and concurrency usage.
- Makes it easier to learn thread-safe design by comparing multiple queue strategies in one codebase.

## Notes

- Most queue classes currently live under `tsfqueue::__impl`.
- The library is header-only, so no separate library linking step is required for usage.
