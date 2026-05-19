# ThreadSafeQueueLib — Examples

This guide walks through every queue type with runnable, self-contained code
snippets. Each example is written for someone who has never used a thread-safe
queue before. All snippets compile with C++17 and the standard `#include
"tsfqueue.hpp"` include path.

---

## Table of Contents

1. [Choosing the right queue](#choosing-the-right-queue)
2. [Example 1 — Lock-free SPSC Bounded](#example-1--lock-free-spsc-bounded)
3. [Example 2 — Lock-free SPSC Unbounded](#example-2--lock-free-spsc-unbounded)
4. [Example 3 — Lock-free MPSC Unbounded](#example-3--lock-free-mpsc-unbounded)
5. [Example 4 — Lock-free MPMC Bounded](#example-4--lock-free-mpmc-bounded)
7. [Example 5 — Blocking MPMC Unbounded](#example-5--blocking-mpmc-unbounded)
8. [Common Pitfalls](#common-pitfalls)
9. [API Quick-Reference](#api-quick-reference)

---

## Choosing the Right Queue

| Situation | Recommended queue |
|---|---|
| Exactly **1** producer thread + exactly **1** consumer thread | `lockfree_spsc_bounded` or `lockfree_spsc_unbounded` |
| Several producers + exactly **1** consumer thread | `lockfree_mpsc_unbounded` |
| Several producers + several consumers, fixed max size | `lockfree_mpmc_bounded` |
| Several producers + several consumers, variable/unknown size | `blocking_mpmc_unbounded` |

> **Rule of thumb:** use the most restrictive queue that fits your topology.
> An SPSC queue is faster than an MPMC queue because it makes fewer
> synchronisation assumptions. Only reach for MPMC when you truly need it.

---

## Example 1 — Lock-free SPSC Bounded

**When to use:** You have **one** thread producing data and **one** thread
consuming it, and you know the maximum number of items that can be in flight
at once.

**Real-world analogy:** A single conveyor belt in a factory — one machine
loads items, one machine unloads them. The belt has a fixed number of slots.

### try_push / try_pop (non-blocking)

Both `try_push` and `try_pop` return immediately with `true` on success or
`false` if the queue is full/empty. Use this when you cannot afford to wait.

```cpp
#include <iostream>
#include <thread>
#include "tsfqueue.hpp"

// Capacity is a compile-time constant (template parameter).
// Here we allow at most 1024 items in flight at any moment.
using Queue = tsfqueue::__impl::lockfree_spsc_bounded<int, 1024>;

int main() {
    Queue q;

    // --- Single-threaded sanity check ---
    bool ok = q.try_push(42);
    std::cout << "pushed: " << std::boolalpha << ok << "\n"; // true

    int value = 0;
    ok = q.try_pop(value);
    std::cout << "popped: " << ok << ", value: " << value << "\n"; // true, 42

    // try_pop on an empty queue returns false and does not modify 'value'
    ok = q.try_pop(value);
    std::cout << "empty pop: " << ok << "\n"; // false

    return 0;
}
```

### wait_and_push / wait_and_pop (spin-wait)

These variants spin (busy-wait) until the operation succeeds. They never
return early — use them when you must guarantee every item is processed.

```cpp
#include <iostream>
#include <thread>
#include "tsfqueue.hpp"

using Queue = tsfqueue::__impl::lockfree_spsc_bounded<int, 8>;

int main() {
    Queue q;
    const int TOTAL = 100;

    // Producer thread: push integers 0..99
    std::thread producer([&]() {
        for (int i = 0; i < TOTAL; ++i) {
            q.wait_and_push(i); // waits if the queue is full
        }
    });

    // Consumer thread: pop and print every value
    std::thread consumer([&]() {
        for (int i = 0; i < TOTAL; ++i) {
            int val;
            q.wait_and_pop(val); // waits if the queue is empty
            std::cout << "got: " << val << "\n";
        }
    });

    producer.join();
    consumer.join();
    return 0;
}
```

### Checking size and peeking

```cpp
#include <iostream>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::lockfree_spsc_bounded<std::string, 64> q;

    q.try_push("hello");
    q.try_push("world");

    std::cout << "size: " << q.size() << "\n"; // 2

    std::string front;
    if (q.peek(front)) {
        // peek reads the front element WITHOUT removing it
        std::cout << "front: " << front << "\n"; // hello
    }

    std::cout << "size after peek: " << q.size() << "\n"; // still 2

    return 0;
}
```

> **Note:** `size()` and `peek()` are meaningful only from the **consumer**
> side in an SPSC queue. Calling them from the producer can give a stale
> snapshot.

---

## Example 2 — Lock-free SPSC Unbounded

**When to use:** Same 1-producer / 1-consumer topology as above, but you
cannot predict how many items will accumulate. The queue grows dynamically on
the heap (no capacity limit).

**Real-world analogy:** A to-do list that never runs out of paper — the chef
can keep adding tickets even if the waiter falls behind.

### Basic push / try_pop

Unlike the bounded variant, `push` on an unbounded queue **never fails** (it
allocates a new node). `try_pop` still returns `false` when empty.

```cpp
#include <iostream>
#include <thread>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::lockfree_spsc_unbounded<double> q;

    // Producer: push five measurements
    std::thread producer([&]() {
        for (double v : {1.1, 2.2, 3.3, 4.4, 5.5}) {
            q.push(v); // always succeeds
        }
    });

    producer.join(); // wait for all pushes to finish

    // Consumer: drain the queue
    double val;
    while (q.try_pop(val)) {
        std::cout << "received: " << val << "\n";
    }
    // Output: 1.1, 2.2, 3.3, 4.4, 5.5

    return 0;
}
```

### wait_and_pop — block until an item arrives

```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::lockfree_spsc_unbounded<std::string> q;

    std::thread consumer([&]() {
        std::string msg;
        q.wait_and_pop(msg); // spins here until something is available
        std::cout << "message: " << msg << "\n";
    });

    // Simulate the producer doing some work first
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    q.push("Hello from producer!");

    consumer.join();
    return 0;
}
```

### emplace_back — construct in place

`emplace_back` lets you pass constructor arguments directly without building
a temporary object first.

```cpp
#include <string>
#include <iostream>
#include "tsfqueue.hpp"

struct Event {
    int    id;
    std::string name;
    Event(int i, std::string n) : id(i), name(std::move(n)) {}
};

int main() {
    tsfqueue::__impl::lockfree_spsc_unbounded<Event> q;

    // Construct the Event directly inside the queue node
    q.emplace_back(1, "login");
    q.emplace_back(2, "purchase");

    Event e{0, ""};
    while (q.try_pop(e)) {
        std::cout << "event " << e.id << ": " << e.name << "\n";
    }
    return 0;
}
```

---

## Example 3 — Lock-free MPSC Unbounded

**When to use:** Multiple threads produce work items, but only **one** thread
processes them (a typical worker/logger pattern).

**Real-world analogy:** Many cashiers dropping receipts into a single central
inbox that one accountant processes.

### Multiple producers, one consumer

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "tsfqueue.hpp"

using Queue = tsfqueue::__impl::lockfree_mpsc_unbounded<std::string>;

int main() {
    Queue q;
    const int NUM_PRODUCERS = 4;
    const int ITEMS_EACH    = 5;
    std::atomic<int> total_produced{0};

    // Spawn several producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ITEMS_EACH; ++i) {
                std::string msg = "P" + std::to_string(p)
                                + "-item" + std::to_string(i);
                q.push(msg); // thread-safe: multiple threads can call push()
                total_produced.fetch_add(1);
            }
        });
    }

    // One consumer thread drains everything
    std::thread consumer([&]() {
        int received = 0;
        const int EXPECTED = NUM_PRODUCERS * ITEMS_EACH;
        while (received < EXPECTED) {
            std::string msg;
            if (q.try_pop(msg)) {
                std::cout << "consumed: " << msg << "\n";
                ++received;
            } else {
                std::this_thread::yield(); // be polite when empty
            }
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();

    std::cout << "all done, queue empty: " << std::boolalpha << q.empty() << "\n";
    return 0;
}
```

### Using wait_and_pop for a blocking consumer

A blocking consumer is simpler to write when you do not want to poll:

```cpp
#include <iostream>
#include <thread>
#include <atomic>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::lockfree_mpsc_unbounded<int> q;
    const int TOTAL = 20;
    std::atomic<int> sent{0};

    // Two producers
    auto produce = [&](int base) {
        for (int i = 0; i < TOTAL / 2; ++i) {
            q.push(base + i);
            sent.fetch_add(1);
        }
    };
    std::thread p1(produce, 0);
    std::thread p2(produce, 100);

    // One consumer using wait_and_pop
    std::thread consumer([&]() {
        for (int i = 0; i < TOTAL; ++i) {
            int val;
            q.wait_and_pop(val); // blocks until an item is available
            std::cout << "got: " << val << "\n";
        }
    });

    p1.join();
    p2.join();
    consumer.join();
    return 0;
}
```

> **Important:** `wait_and_pop` here spins (busy-waits). It does not put the
> thread to sleep the way a mutex/condvar would. This keeps latency low but
> uses CPU. If you need true sleep-and-wake, use the blocking MPMC queue
> (Example 5).

---

## Example 4 — Lock-free MPMC Bounded

**When to use:** Many producers **and** many consumers share a fixed-size ring
buffer. You want the highest throughput and you know the maximum number of
in-flight items at compile time.

**Real-world analogy:** A fixed-size parking lot shared by many drivers
(producers) who drop off packages and many couriers (consumers) who pick them
up.

### try_push / try_pop

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "tsfqueue.hpp"

// A ring buffer with room for 256 items
using Queue = tsfqueue::__impl::lockfree_mpmc_bounded<int, 256>;

int main() {
    Queue q;

    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PROD = 10000;
    const int TOTAL = NUM_PRODUCERS * ITEMS_PER_PROD;

    std::atomic<long long> sum{0};

    // Producers
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&]() {
            for (int i = 1; i <= ITEMS_PER_PROD; ++i) {
                // Spin until there is space
                while (!q.try_push(i)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Consumers
    std::atomic<int> consumed{0};
    std::vector<std::thread> consumers;
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            while (consumed.load(std::memory_order_relaxed) < TOTAL) {
                int val;
                if (q.try_pop(val)) {
                    sum.fetch_add(val, std::memory_order_relaxed);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    std::cout << "sum: " << sum.load() << "\n";
    std::cout << "queue empty: " << std::boolalpha << q.empty() << "\n";
    return 0;
}
```

### wait_and_push / wait_and_pop

If you prefer to let the queue manage the spin loop for you:

```cpp
#include <iostream>
#include <thread>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::lockfree_mpmc_bounded<std::string, 4> q;

    std::thread producer([&]() {
        for (auto& s : {"alpha", "beta", "gamma", "delta", "epsilon"}) {
            q.wait_and_push(s); // blocks (spins) if queue is full
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < 5; ++i) {
            std::string val;
            q.wait_and_pop(val); // blocks (spins) if queue is empty
            std::cout << val << "\n";
        }
    });

    producer.join();
    consumer.join();
    return 0;
}
```

### Capacity enforcement

A bounded queue gives you **backpressure** for free: `try_push` returns
`false` when the ring buffer is full, so producers naturally slow down.

```cpp
#include <iostream>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::lockfree_mpmc_bounded<int, 4> q;

    // Fill to capacity
    for (int i = 1; i <= 4; ++i) q.try_push(i);

    // One more push fails because the ring buffer is full
    bool pushed = q.try_push(99);
    std::cout << "push when full: " << std::boolalpha << pushed << "\n"; // false

    // Pop one item to make room
    int val;
    q.try_pop(val);
    std::cout << "popped: " << val << "\n"; // 1

    // Now the push succeeds
    pushed = q.try_push(99);
    std::cout << "push after pop: " << pushed << "\n"; // true

    return 0;
}
```

---

## Example 5 — Blocking MPMC Unbounded

**When to use:** Many producers, many consumers, and you want the consumer
threads to **sleep** (not spin) when the queue is empty. This saves CPU at the
cost of slightly higher wake-up latency. The queue grows dynamically — there
is no capacity limit.

**Real-world analogy:** A help-desk ticket system. Support agents (consumers)
sleep at their desks. When a customer (producer) submits a ticket, one agent
is woken up automatically.

### Simple push / wait_and_pop

```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::blocking_mpmc_unbounded<int> q;

    // Consumer sleeps until something arrives
    std::thread consumer([&]() {
        int val;
        std::cout << "consumer: waiting...\n";
        q.wait_and_pop(val); // thread is put to sleep by the OS
        std::cout << "consumer: woke up, got " << val << "\n";
    });

    // Simulate producer doing some setup work
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "producer: pushing 42\n";
    q.push(42);

    consumer.join();
    return 0;
}
```

### try_pop (non-blocking check)

```cpp
#include <iostream>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::blocking_mpmc_unbounded<std::string> q;

    // try_pop returns false immediately if the queue is empty
    std::string val;
    bool ok = q.try_pop(val);
    std::cout << "empty pop: " << std::boolalpha << ok << "\n"; // false

    q.push("hi");
    ok = q.try_pop(val);
    std::cout << "got: " << ok << " -> " << val << "\n"; // true -> hi
    return 0;
}
```

### wait_and_pop returning a shared_ptr

The `blocking_mpmc_unbounded` queue offers an alternative overload that
returns a `shared_ptr<T>`. This is useful when the caller needs to share
ownership of the value.

```cpp
#include <iostream>
#include <memory>
#include "tsfqueue.hpp"

int main() {
    tsfqueue::__impl::blocking_mpmc_unbounded<int> q;

    q.push(7);

    // Returns std::shared_ptr<int> — nullptr if empty (try_pop variant)
    auto ptr = q.try_pop();
    if (ptr) {
        std::cout << "value: " << *ptr << "\n"; // 7
    }

    // Returns std::shared_ptr<int> — blocks until available (wait_and_pop variant)
    q.push(99);
    auto blocking_ptr = q.wait_and_pop();
    std::cout << "blocking value: " << *blocking_ptr << "\n"; // 99

    return 0;
}
```

### Full MPMC worker pool pattern

This is the classic thread-pool pattern: a set of worker threads sleep on the
queue and wake up to process tasks as they arrive.

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include "tsfqueue.hpp"

// A simple task is just a callable
using Task  = std::function<void()>;
using Queue = tsfqueue::__impl::blocking_mpmc_unbounded<Task>;

int main() {
    Queue q;
    const int NUM_WORKERS = 3;
    std::atomic<bool> running{true};
    std::atomic<int> completed{0};

    // Worker threads: sleep until a task arrives
    std::vector<std::thread> workers;
    for (int w = 0; w < NUM_WORKERS; ++w) {
        workers.emplace_back([&]() {
            while (running.load()) {
                Task task;
                if (q.try_pop(task)) {
                    task();           // execute the task
                    completed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Submit 9 tasks from the main thread
    const int NUM_TASKS = 9;
    for (int i = 0; i < NUM_TASKS; ++i) {
        q.push([i]() {
            std::cout << "task " << i << " running on thread "
                      << std::this_thread::get_id() << "\n";
        });
    }

    // Wait until all tasks are consumed
    while (completed.load() < NUM_TASKS) {
        std::this_thread::yield();
    }
    running.store(false);

    for (auto& w : workers) w.join();
    std::cout << "all " << NUM_TASKS << " tasks done\n";
    return 0;
}
```

### emplace_back — avoid redundant copies

```cpp
#include <string>
#include <iostream>
#include "tsfqueue.hpp"

struct LogEntry {
    int         level;
    std::string message;
    LogEntry(int l, std::string m) : level(l), message(std::move(m)) {}
};

int main() {
    tsfqueue::__impl::blocking_mpmc_unbounded<LogEntry> q;

    // Construct the LogEntry directly inside the queue — no copy needed
    q.emplace_back(1, "server started");
    q.emplace_back(2, "connection accepted");

    LogEntry entry{0, ""};
    while (q.try_pop(entry)) {
        std::cout << "[level " << entry.level << "] " << entry.message << "\n";
    }
    return 0;
}
```

---

## Common Pitfalls

### 1. Using SPSC with more than one producer or consumer

```cpp
// WRONG — two threads calling try_push() on an SPSC queue is a data race
std::thread p1([&]() { q.try_push(1); });
std::thread p2([&]() { q.try_push(2); }); // undefined behaviour!
```

If you have multiple producers, switch to the MPSC or MPMC variant.

### 2. Dropping items silently with try_push

```cpp
// WRONG — you might silently lose items if the queue is full
q.try_push(value); // return value ignored!

// CORRECT — always check or use wait_and_push
if (!q.try_push(value)) {
    // handle the full-queue case: retry, log, or drop intentionally
}
```

### 3. Busy-spinning without yielding

```cpp
// BAD — burns a full CPU core while waiting
while (!q.try_pop(val)) { /* nothing */ }

// BETTER — yield to the scheduler so other threads can run
while (!q.try_pop(val)) {
    std::this_thread::yield();
}
```

### 4. Peeking from the producer side (SPSC)

`peek()` and `size()` are designed for the consumer side in SPSC queues.
Reading them from the producer thread may return stale data because the cache
lines are owned by the consumer.

### 5. Forgetting to join threads

Always call `t.join()` (or `t.detach()`) before the queue goes out of scope.
A thread touching a destroyed queue is undefined behaviour.

```cpp
// WRONG — producer might still be running when 'q' is destroyed
{
    Queue q;
    std::thread producer([&]() { /* ... */ });
    // producer.join() missing!
} // q destroyed here — CRASH
```

---

## API Quick-Reference

| Method | SPSC bounded | SPSC unbounded | MPSC unbounded | MPMC bounded | Blocking MPMC |
|---|:---:|:---:|:---:|:---:|:---:|
| `push(value)` | — | ✓ | ✓ | — | ✓ |
| `try_push(value)` | ✓ | — | — | ✓ | — |
| `wait_and_push(value)` | ✓ | — | — | ✓ | — |
| `emplace_back(args...)` | — | ✓ | ✓ | ✓ | ✓ |
| `try_pop(ref)` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `try_pop()` → `shared_ptr` | — | — | — | — | ✓ |
| `wait_and_pop(ref)` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `wait_and_pop()` → `shared_ptr` | — | — | — | — | ✓ |
| `peek(ref)` | ✓ | ✓ | ✓ | — | — |
| `empty()` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `size()` | ✓ | ✓ | ✓ | ✓ | ✓ |

**Return-value semantics at a glance:**
- `push` / `wait_and_push` / `wait_and_pop` — always succeed (spin or sleep
  until they can).
- `try_push` / `try_pop` / `peek` — return `bool`: `true` = success,
  `false` = queue full or empty.
- `try_pop()` / `wait_and_pop()` (no argument) on the blocking MPMC queue —
  return `std::shared_ptr<T>` (`nullptr` means empty).
